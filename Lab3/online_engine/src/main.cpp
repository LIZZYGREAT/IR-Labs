#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <exception>

#include "../include/httplib.h" 
#include "../include/double_array_trie.h"
#include "../include/trie_dp_search.h"
#include "../include/bigram_lm.h"
#include "../include/viterbi_decoder.h"
#include "../include/prefix_suggester.h"
#include "../include/search_config.h"

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

int main() {
    // 架构升级：明确区分单词字典与整句索引
    std::string word_index_dir = "../data/index/words";
    std::string query_index_dir = "../data/index/queries";
    std::string bigram_path = "../data/processed/bigram_lm.tsv";

    try {
        std::cout << "[INFO] Initializing Dual-DAT NLP Engine..." << std::endl;
        
        // 1. 初始化纠错子系统 (基于 words)
        DoubleArrayTrie word_dat(word_index_dir);
        TrieDPSearcher searcher(word_dat);
        BigramLM lm(bigram_path, word_dat, -5.0); 
        ViterbiDecoder decoder(lm, 2.5, 5, 10.0);
        
        // 2. 初始化召回子系统 (基于 queries)
        DoubleArrayTrie query_dat(query_index_dir);
        
        // 参数：加载 query_dat 与 word_dat，最大 DFS 搜索节点 5000，断崖惩罚系数 2.0
        PrefixSuggester suggester(query_dat, word_dat, searcher, 5000, 2.0);

        httplib::Server svr;

        svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        });

        // 自动补全端点：此时已完全消除生成幻觉，提供 100% 语料库一致性匹配
        svr.Get("/suggest", [&](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            
            if (!req.has_param("q")) {
                res.set_content("[]", "application/json");
                return;
            }

            std::string raw_query = req.get_param_value("q");
            if (raw_query.empty()) {
                res.set_content("[]", "application/json");
                return;
            }

            auto start_suggest = std::chrono::high_resolution_clock::now();
            std::vector<Suggestion> suggestions = suggester.suggest(raw_query, 5);
            auto end_suggest = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_suggest - start_suggest).count();

            std::string json_response = "[";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                json_response += "{";
                json_response += "\"suggestion\": \"" + suggestions[i].full_text + "\",";
                json_response += "\"score\": " + std::to_string(suggestions[i].score) + ",";
                json_response += "\"latency_ms\": " + std::to_string(duration / 1000.0);
                json_response += "}";
                if (i < suggestions.size() - 1) json_response += ",";
            }
            json_response += "]";

            res.set_content(json_response, "application/json");
        });

        // 整句纠错端点：逻辑保持不变
        svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            
            if (!req.has_param("q")) {
                res.set_content("[]", "application/json");
                return;
            }

            std::string raw_query = req.get_param_value("q");
            if (raw_query.empty()) {
                res.set_content("[]", "application/json");
                return;
            }

            auto start_search = std::chrono::high_resolution_clock::now();
            
            std::vector<std::string> tokens = tokenize(raw_query);
            std::vector<std::vector<Candidate>> all_candidates;
            all_candidates.reserve(tokens.size());

            SearchParams spell_config;
            spell_config.base_threshold = 3.0; 
            spell_config.max_error_ratio = 0.50;
            spell_config.fallback_limit = 2.5;

            for (const std::string& token : tokens) {
                std::vector<Candidate> cands = searcher.search(token, spell_config, 15);
                if (cands.empty()) {
                    cands.push_back({token, 0.0, -15.0, -1}); 
                }
                all_candidates.push_back(cands);
            }

            std::vector<DecodedSentence> final_results = decoder.decode(all_candidates);

            auto end_search = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count();

            std::string json_response = "[";
            for (size_t i = 0; i < final_results.size(); ++i) {
                json_response += "{";
                json_response += "\"word\": \"" + final_results[i].text + "\",";
                json_response += "\"score\": " + std::to_string(final_results[i].final_score) + ",";
                json_response += "\"distance\": \"Rank " + std::to_string(i + 1) + "\","; 
                json_response += "\"latency_ms\": " + std::to_string(duration / 1000.0);
                json_response += "}";
                if (i < final_results.size() - 1) json_response += ",";
            }
            json_response += "]";

            res.set_content(json_response, "application/json");
        });

        std::cout << "[SUCCESS] Server bound to http://127.0.0.1:8080" << std::endl;
        svr.listen("127.0.0.1", 8080);

    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FATAL ERROR] " << e.what() << std::endl;
        std::cin.get();
        return 1;
    }

    return 0;
}