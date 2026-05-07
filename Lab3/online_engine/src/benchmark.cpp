#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../include/double_array_trie.h"
#include "../include/trie_dp_search.h"
#include "../include/search_config.h"

// 定义测试用例结构体：对应 Python 脚本生成的平行语料
struct TestCase {
    std::string noisy_query; // 带有键盘物理噪声的查询 (观测态)
    std::string true_intent; // 原始干净的查询 (真实意图)
};

// 核心评估函数：执行单次参数的网格扫描
void run_benchmark_for_config(
    const std::vector<TestCase>& test_cases, 
    const TrieDPSearcher& searcher, 
    const SearchParams& params, 
    int top_k
) {
    int total_queries = test_cases.size();
    double total_mrr = 0.0;
    int hits_at_k = 0;
    
    std::vector<long long> latencies_us;
    latencies_us.reserve(total_queries);

    auto global_start = std::chrono::high_resolution_clock::now();

    for (const auto& tc : test_cases) {
        auto start_search = std::chrono::high_resolution_clock::now();

        // 显式传入上下文配置进行搜索，获取 Top-K 候选
        std::vector<Candidate> candidates = searcher.search(tc.noisy_query, params, top_k);

        auto end_search = std::chrono::high_resolution_clock::now();
        latencies_us.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count());

        int rank = -1;
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].word == tc.true_intent) {
                rank = i + 1;
                break;
            }
        }

        // 计算 MRR 和 Recall
        if (rank > 0) total_mrr += 1.0 / rank;
        if (rank > 0 && rank <= top_k) hits_at_k++;
    }

    auto global_end = std::chrono::high_resolution_clock::now();
    double duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_start).count() / 1000.0;

    // 统计长尾延迟
    std::sort(latencies_us.begin(), latencies_us.end());
    long long p50_latency = latencies_us[total_queries * 0.50];
    long long p90_latency = latencies_us[total_queries * 0.90];
    long long p99_latency = latencies_us[total_queries * 0.99];

    double mrr_score = total_mrr / total_queries;
    double recall_at_k = (static_cast<double>(hits_at_k) / total_queries) * 100.0;
    double qps = total_queries / duration_sec;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "| Fuse:" << std::setw(6) << params.confidence_fuse 
              << " | Ratio:" << std::setw(4) << params.max_error_ratio 
              << " | MRR: " << std::setw(4) << mrr_score 
              << " | R@" << top_k << ": " << std::setw(5) << recall_at_k << "%"
              << " | P50: " << std::setw(4) << p50_latency / 1000.0 << "ms"
              << " | P99: " << std::setw(4) << p99_latency / 1000.0 << "ms"
              << " | QPS: " << std::setw(6) << qps << " |" << std::endl;
}

int main() {
    std::cout << "==========================================================================" << std::endl;
    std::cout << "          IR Candidate Generator Parameter Sweep Benchmark" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    std::string index_dir = "../data/index";
    // 指向你通过 noise_generator.py 生成的文件
    std::string test_file = "../data/processed/msmarco_parallel_corpus.tsv"; 
    
    DoubleArrayTrie dat(index_dir);
    TrieDPSearcher searcher(dat);
    
    std::vector<TestCase> test_cases;
    std::ifstream infile(test_file);

    if (!infile.is_open()) {
        std::cerr << "[FATAL] Cannot open test file: " << test_file << std::endl;
        std::cerr << "        Please ensure you have run 'noise_generator.py' first." << std::endl;
        return 1;
    }

    std::string line;
    bool is_first_line = true;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        
        // 兼容处理 Python 脚本写入的 "noisy_query\tclean_query\n" 表头
        if (is_first_line) {
            is_first_line = false;
            if (line.find("noisy_query") != std::string::npos) {
                continue; 
            }
        }

        size_t tab_pos = line.find('\t');
        if (tab_pos != std::string::npos) {
            test_cases.push_back({line.substr(0, tab_pos), line.substr(tab_pos + 1)});
        }
    }
    infile.close();

    if (test_cases.empty()) {
        std::cerr << "[ERROR] Test set is empty!" << std::endl;
        return 1;
    }
    std::cout << "[INFO] Loaded Test Cases: " << test_cases.size() << std::endl;
    std::cout << "[INFO] Initiating Grid Search for Confidence Fuse & Error Ratio...\n" << std::endl;

    // 构建待测试的超参数网格
    std::vector<SearchParams> grid;
    std::vector<double> fuses = {-12.0, -15.0, -18.0}; 
    std::vector<double> ratios = {0.35, 0.45, 0.55};   

    for (double f : fuses) {
        for (double r : ratios) {
            SearchParams p;
            p.confidence_fuse = f;
            p.max_error_ratio = r;
            p.base_threshold = 2.5; 
            p.fallback_limit = 2.0; 
            p.dist_weight = 2.0;
            p.length_compensation = 0.2;
            grid.push_back(p);
        }
    }

    // 针对每一组参数，执行全量测试集扫描，评估 Top-5 候选质量
    for (const auto& p : grid) {
        run_benchmark_for_config(test_cases, searcher, p, 5); 
    }

    std::cout << "==========================================================================" << std::endl;
    return 0;
}