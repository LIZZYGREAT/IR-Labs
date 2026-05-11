#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <unordered_map>
#include <list>
#include <optional>
#include <thread>
#include <atomic>
#include "../include/double_array_trie.h"
#include "../include/trie_dp_search.h"
#include "../include/search_config.h"

// ==========================================
// 1. 无锁 LRU 缓存 (保持不变)
// ==========================================
struct CacheKey {
    std::string query;
    double threshold;
    double ratio;

    bool operator==(const CacheKey& other) const {
        return query == other.query && 
               std::abs(threshold - other.threshold) < 1e-6 && 
               std::abs(ratio - other.ratio) < 1e-6;
    }
};

struct CacheKeyHash {
    std::size_t operator()(const CacheKey& k) const {
        std::size_t h1 = std::hash<std::string>{}(k.query);
        std::size_t h2 = std::hash<double>{}(k.threshold);
        std::size_t h3 = std::hash<double>{}(k.ratio);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class QueryCache {
private:
    size_t capacity;
    std::list<std::pair<CacheKey, std::vector<Candidate>>> items;
    std::unordered_map<CacheKey, decltype(items)::iterator, CacheKeyHash> cache_map;

public:
    QueryCache(size_t cap = 5000) : capacity(cap) {}

    std::optional<std::vector<Candidate>> get(const CacheKey& key) {
        auto it = cache_map.find(key);
        if (it == cache_map.end()) return std::nullopt;
        items.splice(items.begin(), items, it->second);
        return it->second->second;
    }

    void put(const CacheKey& key, const std::vector<Candidate>& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            it->second->second = value;
            items.splice(items.begin(), items, it->second);
            return;
        }
        if (items.size() >= capacity) {
            cache_map.erase(items.back().first);
            items.pop_back();
        }
        items.push_front({key, value});
        cache_map[key] = items.begin();
    }
};

// ==========================================
// 2. 加权 Token 级评测组件
// ==========================================
struct WeightedTestCase {
    std::string noisy_token; 
    std::string clean_token; 
    long long weight; 

    // 用于按频次降序排序
    bool operator<(const WeightedTestCase& other) const {
        return weight > other.weight; 
    }
};

void run_benchmark_for_config(
    const std::vector<WeightedTestCase>& test_cases, 
    const TrieDPSearcher& searcher, 
    std::vector<QueryCache>& sharded_caches, 
    const SearchParams& params, 
    int top_k,
    std::ofstream& csv_file
) {
    int total_unique_cases = test_cases.size();
    if (total_unique_cases == 0) return;

    long long total_actual_queries = 0;
    for (const auto& tc : test_cases) {
        total_actual_queries += tc.weight;
    }

    int num_threads = sharded_caches.size(); 

    std::vector<double> thread_mrr(num_threads, 0.0);
    std::vector<long long> thread_hits(num_threads, 0);
    std::vector<long long> thread_cache_hits(num_threads, 0);
    std::vector<std::vector<long long>> thread_latencies(num_threads);
    
    std::atomic<int> progress_counter{0};

    auto worker = [&](int thread_id, size_t start_idx, size_t end_idx) {
        thread_latencies[thread_id].reserve(end_idx - start_idx);
        QueryCache& local_cache = sharded_caches[thread_id];

        for (size_t i = start_idx; i < end_idx; ++i) {
            const auto& tc = test_cases[i];
            CacheKey key = {tc.noisy_token, params.base_threshold, params.max_error_ratio};
            
            auto start_search = std::chrono::high_resolution_clock::now();
            
            std::vector<Candidate> candidates;
            auto cached_res = local_cache.get(key);
            
            if (cached_res) {
                candidates = *cached_res;
                thread_cache_hits[thread_id] += tc.weight; 
            } else {
                candidates = searcher.search(tc.noisy_token, params, top_k);
                local_cache.put(key, candidates);
            }

            auto end_search = std::chrono::high_resolution_clock::now();
            long long latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count();
            thread_latencies[thread_id].push_back(latency_us);

            int rank = -1;
            for (size_t j = 0; j < candidates.size(); ++j) {
                if (candidates[j].word == tc.clean_token) {
                    rank = j + 1;
                    break;
                }
            }

            if (rank > 0) thread_mrr[thread_id] += (1.0 / rank) * tc.weight;
            if (rank > 0 && rank <= top_k) thread_hits[thread_id] += tc.weight;

            int current_progress = ++progress_counter;
            if (thread_id == 0 && (current_progress % 1000 == 0 || current_progress == total_unique_cases)) {
                int percent = (current_progress * 100) / total_unique_cases;
                std::cout << "\r[Evaluating] Fuse: " << std::setw(5) << params.confidence_fuse 
                          << " | Ratio: " << std::setw(4) << params.max_error_ratio 
                          << " | Progress: " << std::setw(3) << percent << "% (" 
                          << current_progress << "/" << total_unique_cases << ")" << std::flush;
            }
        }
    };

    auto global_start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    size_t chunk_size = total_unique_cases / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? total_unique_cases : start + chunk_size;
        threads.emplace_back(worker, i, start, end);
    }

    for (auto& t : threads) t.join();

    auto global_end = std::chrono::high_resolution_clock::now();
    double duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_start).count() / 1000.0;

    double total_mrr_sum = 0.0;
    long long total_hits_sum = 0;
    long long total_cache_hits = 0;
    std::vector<long long> all_latencies;
    all_latencies.reserve(total_unique_cases);

    for (int i = 0; i < num_threads; ++i) {
        total_mrr_sum += thread_mrr[i];
        total_hits_sum += thread_hits[i];
        total_cache_hits += thread_cache_hits[i];
        all_latencies.insert(all_latencies.end(), thread_latencies[i].begin(), thread_latencies[i].end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());
    long long p99_latency = all_latencies[total_unique_cases * 0.99];

    double mrr_score = total_mrr_sum / static_cast<double>(total_actual_queries);
    double recall_at_k = (static_cast<double>(total_hits_sum) / total_actual_queries) * 100.0;
    double qps = total_actual_queries / duration_sec;
    double hit_rate = (static_cast<double>(total_cache_hits) / total_actual_queries) * 100.0;

    std::cout << "\r" << std::string(85, ' ') << "\r"; 
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "| Fuse:" << std::setw(6) << params.confidence_fuse 
              << " | Ratio:" << std::setw(4) << params.max_error_ratio 
              << " | MRR:" << std::setw(5) << mrr_score 
              << " | R@" << top_k << ":" << std::setw(5) << recall_at_k << "%"
              << " | P99:" << std::setw(5) << p99_latency / 1000.0 << "ms"
              << " | Eq.QPS:" << std::setw(8) << qps 
              << " | CacheHit:" << std::setw(5) << hit_rate << "% |" << std::endl;

    if (csv_file.is_open()) {
        csv_file << std::fixed << std::setprecision(4)
                 << params.confidence_fuse << ","
                 << params.max_error_ratio << ","
                 << mrr_score << ","
                 << recall_at_k << ","
                 << (p99_latency / 1000.0) << ","
                 << qps << ","
                 << hit_rate << "\n";
        csv_file.flush(); 
    }
}

int main() {
    std::cout << "==========================================================================" << std::endl;
    std::cout << "    IR Candidate Generator Benchmark (Top-N Truncated Evaluation)" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    int num_cores = 8;
    std::cout << "[INFO] Forcing execution with " << num_cores << " threads." << std::endl;


    const size_t MAX_TEST_CASES = 80000; 

    std::string index_dir = "../data/index";
    std::string test_file = "../data/processed/weighted_benchmark_tokens.tsv";
    std::string csv_output_file = "../data/processed/benchmark_results.csv";
    
    DoubleArrayTrie dat(index_dir);
    TrieDPSearcher searcher(dat);
    std::vector<QueryCache> sharded_caches(num_cores, QueryCache(10000));
    
    std::vector<WeightedTestCase> all_cases;
    std::ifstream infile(test_file);
    if (!infile.is_open()) {
        std::cerr << "[FATAL] Cannot open test file: " << test_file << std::endl;
        return 1;
    }

    std::string line;
    bool is_header = true;
    long long global_total_volume = 0; // 记录全量 112 万条数据的真实总频次

    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        if (is_header) { is_header = false; continue; } 
        
        size_t first_tab = line.find('\t');
        size_t second_tab = line.find('\t', first_tab + 1);
        
        if (first_tab != std::string::npos && second_tab != std::string::npos) {
            std::string noisy = line.substr(0, first_tab);
            std::string clean = line.substr(first_tab + 1, second_tab - first_tab - 1);
            long long weight = std::stoll(line.substr(second_tab + 1));
            
            all_cases.push_back({noisy, clean, weight});
            global_total_volume += weight;
        }
    }
    infile.close();

    std::sort(all_cases.begin(), all_cases.end());

    size_t cases_to_keep = std::min(MAX_TEST_CASES, all_cases.size());
    std::vector<WeightedTestCase> test_cases(all_cases.begin(), all_cases.begin() + cases_to_keep);

    long long truncated_volume = 0;
    for (const auto& tc : test_cases) {
        truncated_volume += tc.weight;
    }

    double coverage_ratio = (static_cast<double>(truncated_volume) / global_total_volume) * 100.0;

    std::cout << "[INFO] Dataset loaded. Total unique errors: " << all_cases.size() << std::endl;
    std::cout << "[INFO] Applied Head-Query Truncation (Top " << cases_to_keep << " cases)." << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[INFO] Captured Volume Coverage: " << coverage_ratio << "% of global physical traffic." << std::endl;
    std::cout << "--------------------------------------------------------------------------" << std::endl;
    
    std::ofstream csv_file(csv_output_file);
    if (csv_file.is_open()) {
        csv_file << "confidence_fuse,max_error_ratio,MRR,Recall_at_5(%),P99_Latency(ms),Eq_QPS,Cache_Hit_Rate(%)\n";
    }

    std::vector<double> fuses = {-12.0, -15.0, -18.0}; 
    std::vector<double> ratios = {0.35, 0.45, 0.55};   

    for (double f : fuses) {
        for (double r : ratios) {
            SearchParams p;
            p.confidence_fuse = f;
            p.max_error_ratio = r;
            p.base_threshold = 2.5;
            p.length_compensation = 0.2;
            p.dist_weight = 2.0;
            p.fallback_limit = 2.0;

            run_benchmark_for_config(test_cases, searcher, sharded_caches, p, 5, csv_file); 
        }
    }

    if (csv_file.is_open()) csv_file.close();
    std::cout << "==========================================================================" << std::endl;
    return 0;
}