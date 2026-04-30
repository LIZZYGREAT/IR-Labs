#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <future>
#include <exception>

#include "../include/httplib.h" 
#include "../include/double_array_trie.h"
#include "../include/trie_dp_search.h"
#include "../include/bigram_lm.h"
#include "../include/viterbi_decoder.h"

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
    std::string index_dir = "../data/index";
    std::string bigram_path = "../data/processed/bigram_lm.tsv";

    try {
        std::cout << "[INFO] Initializing NLP Engine with Beam Search..." << std::endl;
        
        DoubleArrayTrie dat(index_dir);
        TrieDPSearcher searcher(dat);
        BigramLM lm(bigram_path, dat, -5.0); 
        
        // 初始化解码器：物理衰减 2.5，输出 Top-5 路径，精确匹配奖励 10.0
        ViterbiDecoder decoder(lm, 2.5, 5, 10.0);

        httplib::Server svr;

        svr.Options("/search", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        });

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

            std::vector<std::future<std::vector<Candidate>>> futures;
            futures.reserve(tokens.size());

            for (const std::string& token : tokens) {
                futures.push_back(std::async(std::launch::async, [&searcher, token]() {
                    // 放宽底层字典的召回数量，供给 Beam Search 充足的组合燃料
                    std::vector<Candidate> cands = searcher.search(token, 3.0, 15);
                    if (cands.empty()) {
                        cands.push_back({token, 0.0, -15.0, -1}); 
                    }
                    return cands;
                }));
            }

            for (auto& f : futures) {
                all_candidates.push_back(f.get());
            }

            // 接收 Top-K 结果集合
            std::vector<DecodedSentence> final_results = decoder.decode(all_candidates);

            auto end_search = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count();

            // 动态构建 JSON 数组
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