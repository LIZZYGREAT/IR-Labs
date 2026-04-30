#include "../include/bigram_lm.h"
#include <fstream>
#include <iostream>
#include <sstream>

BigramLM::BigramLM(const std::string& tsv_filepath, const DoubleArrayTrie& dat, double backoff_penalty_val) 
    : backoff_penalty(backoff_penalty_val) {
    
    std::ifstream infile(tsv_filepath);
    if (!infile.is_open()) {
        std::cerr << "[ERROR] Failed to open Bigram LM file: " << tsv_filepath << std::endl;
        return;
    }

    std::string line;
    std::getline(infile, line); 

    int count = 0;
    int oov_count = 0;

    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        size_t first_tab = line.find('\t');
        size_t second_tab = line.find('\t', first_tab + 1);

        if (first_tab != std::string::npos && second_tab != std::string::npos) {
            std::string bigram = line.substr(0, first_tab);
            double log_p = std::stod(line.substr(second_tab + 1));
            
            // 预编译期间拆分单词
            size_t space_pos = bigram.find(' ');
            if (space_pos != std::string::npos) {
                std::string w1 = bigram.substr(0, space_pos);
                std::string w2 = bigram.substr(space_pos + 1);

                // 通过 DAT 将字符串降维为底层状态 ID
                int state1 = dat.get_word_state(w1);
                int state2 = dat.get_word_state(w2);

                // 只有当两个词都在 DAT 中有效时才加载转移概率
                if (state1 != -1 && state2 != -1) {
                    uint64_t key = pack_key(state1, state2);
                    transitions[key] = log_p;
                    count++;
                } else {
                    oov_count++;
                }
            }
        }
    }
    std::cout << "[INFO] LM loaded " << count << " compressed Bigram transitions. (Skipped " << oov_count << " OOV entries)" << std::endl;
}

double BigramLM::get_transition_log_p(int state1, int state2, double unigram_log_p) const {
    // 保护：如果存在无法解析的状态直接回退
    if (state1 == -1 || state2 == -1) {
        return unigram_log_p + backoff_penalty;
    }

    uint64_t key = pack_key(state1, state2);
    auto it = transitions.find(key);
    
    if (it != transitions.end()) {
        return it->second;
    } else {
        return unigram_log_p + backoff_penalty;
    }
}