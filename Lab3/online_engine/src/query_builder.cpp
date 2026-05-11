#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

// 业务逻辑依赖
#include "../include/double_array_trie.h"
#include "../include/bigram_lm.h"
// 引入解耦后的基础构建组件
#include "../include/dat_builder.h" 

// 简单的标点符号清洗分词器
std::vector<std::string> tokenize_query(const std::string& text) {
    std::string clean_text = text;
    std::replace_if(clean_text.begin(), clean_text.end(), [](char c) {
        return ispunct(static_cast<unsigned char>(c)) && c != '\''; 
    }, ' ');

    std::vector<std::string> tokens;
    std::istringstream iss(clean_text);
    std::string word;
    while (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        tokens.push_back(word);
    }
    return tokens;
}

/**
 * @brief 计算句子的内禀流畅度与基础权重混合得分
 */
double calculate_intrinsic_weight(
    const std::vector<std::string>& tokens, 
    int count, 
    const DoubleArrayTrie& word_dat, 
    const BigramLM& lm,
    double alpha = 0.5
) {
    if (tokens.empty()) return -100.0;

    double log_count = std::log(std::max(count, 1));
    
    if (tokens.size() == 1) {
        int state = word_dat.get_word_state(tokens[0]);
        double prior = (state != -1) ? word_dat.get_max_weight(state) : -20.0;
        return alpha * log_count + (1.0 - alpha) * prior;
    }

    double chain_log_p = 0.0;
    int valid_transitions = 0;

    for (size_t i = 1; i < tokens.size(); ++i) {
        int state1 = word_dat.get_word_state(tokens[i - 1]);
        int state2 = word_dat.get_word_state(tokens[i]);

        double prior_w2 = (state2 != -1) ? word_dat.get_max_weight(state2) : -20.0;
        double transition_p = lm.get_transition_log_p(state1, state2, prior_w2);
        
        chain_log_p += transition_p;
        valid_transitions++;
    }

    double avg_fluency = chain_log_p / std::max(valid_transitions, 1);
    
    return (alpha * log_count) + ((1.0 - alpha) * avg_fluency);
}

int main() {
    std::string word_index_dir = "../data/index/words";
    std::string bigram_path = "../data/processed/bigram_lm.tsv";
    std::string query_log_file = "../data/raw/query_logs.tsv";
    std::string query_index_dir = "../data/index/queries";

    try {
        std::cout << "[PHASE 1] Bootstrapping Language Models for Fluency Calculation..." << std::endl;
        DoubleArrayTrie word_dat(word_index_dir);
        BigramLM lm(bigram_path, word_dat, -5.0);

        std::cout << "[PHASE 2] Processing Query Logs & Computing Intrinsic Weights..." << std::endl;
        ArenaTrie query_arena;
        
        std::ifstream infile(query_log_file);
        if (!infile.is_open()) {
            throw std::runtime_error("Failed to open query logs. Please ensure ../data/raw/query_logs.tsv exists.");
        }

        std::string line;
        int processed_lines = 0;
        
        while (std::getline(infile, line)) {
            if (line.empty()) continue;

            size_t tab_pos = line.find('\t');
            if (tab_pos == std::string::npos) continue;

            std::string query_text = line.substr(0, tab_pos);
            int count = 1;
            try {
                count = std::stoi(line.substr(tab_pos + 1));
            } catch (...) {}

            std::string lower_query = query_text;
            std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

            std::vector<std::string> tokens = tokenize_query(lower_query);
            double final_weight = calculate_intrinsic_weight(tokens, count, word_dat, lm, 0.4);

            query_arena.insert(lower_query, final_weight);
            processed_lines++;
        }
        infile.close();
        std::cout << "[SUCCESS] Processed " << processed_lines << " distinct queries." << std::endl;

        std::cout << "[PHASE 3] Propagating Max Subtree Weights for A* Pruning..." << std::endl;
        query_arena.compute_max_weights(query_arena.root);

        std::cout << "[PHASE 4] Mapping to Double-Array Trie (Query DAT)..." << std::endl;
        // 赋予更大的初始容量，因为包含空格的句子节点远多于单一单词
        DATBuilder builder(10000000); 
        builder.build_from_arena(query_arena);

        std::cout << "[PHASE 5] Exporting Query Index MMAP..." << std::endl;
        builder.export_mmap(query_index_dir);

    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}