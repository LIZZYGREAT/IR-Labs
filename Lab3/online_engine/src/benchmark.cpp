#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../include/double_array_trie.h"
#include "../include/trie_dp_search.h"
#include "../include/re_ranker.h"

// 定义测试用例结构体
struct TestCase {
    std::string noisy_query; // 带有拼写错误或截断的输入
    std::string true_intent; // 真实的搜索意图
};

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "IR Pre-retrieval System Automated Benchmark" << std::endl;
    std::cout << "==========================================" << std::endl;

    // 1. 系统初始化
    std::string index_dir = "../data/index";
    DoubleArrayTrie dat(index_dir);
    TrieDPSearcher searcher(dat);
    ReRanker reranker(2.5); // 衰减系数保持一致

    // 2. 加载测试集 (需提前准备 benchmark_queries.tsv)
    std::string test_file = "../data/processed/benchmark_queries.tsv";
    std::vector<TestCase> test_cases;
    std::ifstream infile(test_file);

    if (!infile.is_open()) {
        std::cerr << "Fatal error: cannot open test file " << test_file << std::endl;
        std::cerr << "Please ensure the file exists with format: <noisy_query> \\t <true_intent>" << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        size_t tab_pos = line.find('\t');
        if (tab_pos != std::string::npos) {
            std::string noisy = line.substr(0, tab_pos);
            std::string truth = line.substr(tab_pos + 1);
            test_cases.push_back({noisy, truth});
        }
    }
    infile.close();
    
    int total_queries = test_cases.size();
    if (total_queries == 0) {
        std::cerr << "Test set is empty!" << std::endl;
        return 1;
    }
    std::cout << "Successfully loaded test cases: " << total_queries << std::endl;

    // 3. 定义评测指标变量
    double total_mrr = 0.0;
    int hits_at_5 = 0;
    int noisy_query_count = 0;
    int exact_corrections = 0;
    std::vector<long long> latencies_us; // 记录每次查询的耗时 (微秒)
    latencies_us.reserve(total_queries);

    std::cout << "Executing high-frequency graph search benchmark..." << std::endl;

    // 记录全局起始时间以计算 QPS
    auto global_start = std::chrono::high_resolution_clock::now();

    // 4. 执行核心遍历测试
    for (const auto& tc : test_cases) {
        auto start_search = std::chrono::high_resolution_clock::now();

        // 执行召回与精排
        std::vector<Candidate> candidates = searcher.search(tc.noisy_query, 2.5, 50);
        std::vector<FinalResult> final_results = reranker.rank(candidates);

        auto end_search = std::chrono::high_resolution_clock::now();
        long long duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count();
        latencies_us.push_back(duration_us);

        // 计算排名
        int rank = -1;
        for (size_t i = 0; i < final_results.size(); ++i) {
            if (final_results[i].word == tc.true_intent) {
                rank = i + 1;
                break;
            }
        }

        // 累加 MRR
        if (rank > 0) {
            total_mrr += 1.0 / rank;
        }

        // 累加 Recall@5
        if (rank > 0 && rank <= 5) {
            hits_at_5++;
        }

        // 计算错误修正率 (仅统计输入本身有错拼的情况)
        if (tc.noisy_query != tc.true_intent) {
            noisy_query_count++;
            if (rank == 1) { // 必须排名第一才算成功纠错
                exact_corrections++;
            }
        }
    }

    auto global_end = std::chrono::high_resolution_clock::now();
    double global_duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_start).count() / 1000.0;

    // 5. 统计与计算结果
    std::sort(latencies_us.begin(), latencies_us.end());
    long long p50_latency = latencies_us[total_queries * 0.50];
    long long p90_latency = latencies_us[total_queries * 0.90];
    long long p99_latency = latencies_us[total_queries * 0.99];

    double mrr_score = total_mrr / total_queries;
    double recall_at_5 = (static_cast<double>(hits_at_5) / total_queries) * 100.0;
    double correction_rate = noisy_query_count > 0 ? (static_cast<double>(exact_corrections) / noisy_query_count) * 100.0 : 0.0;
    double qps = total_queries / global_duration_sec;

    // 6. 输出量化报告
    std::cout << "\n==========================================" << std::endl;
    std::cout << "          System Quantitative Evaluation Report" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[Quality Metrics]" << std::endl;
    std::cout << "  - Mean Reciprocal Rank (MRR):      " << mrr_score << std::endl;
    std::cout << "  - K-Recall (Recall@5):             " << recall_at_5 << " %" << std::endl;
    std::cout << "  - Correction Rate:                 " << correction_rate << " % (" << exact_corrections << "/" << noisy_query_count << ")" << std::endl;
    std::cout << "\n[Performance Metrics]" << std::endl;
    std::cout << "  - P50 Median Latency:              " << p50_latency / 1000.0 << " ms" << std::endl;
    std::cout << "  - P90 Tail Latency:                " << p90_latency / 1000.0 << " ms" << std::endl;
    std::cout << "  - P99 Extreme Tail Latency:        " << p99_latency / 1000.0 << " ms" << std::endl;
    std::cout << "  - System Throughput (QPS):         " << qps << " queries/sec" << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}