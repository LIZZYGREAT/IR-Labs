#include "../include/bigram_lm.h"
#include <fstream>
#include <iostream>
#include <algorithm>

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
            
            size_t space_pos = bigram.find(' ');
            if (space_pos != std::string::npos) {
                std::string w1 = bigram.substr(0, space_pos);
                std::string w2 = bigram.substr(space_pos + 1);

                int state1 = dat.get_word_state(w1);
                int state2 = dat.get_word_state(w2);

                if (state1 != -1 && state2 != -1) {
                    uint64_t key = pack_key(state1, state2);
                    transitions[key] = log_p;
                    
                    forward_edges[state1].push_back({state2, log_p});
                    state_to_word[state1] = w1;
                    state_to_word[state2] = w2;
                    
                    count++;
                } else {
                    oov_count++;
                }
            }
        }
    }
    
    for (auto& pair : forward_edges) {
        std::sort(pair.second.begin(), pair.second.end(), 
                  [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                      return a.second > b.second; 
                  });
    }

    std::cout << "[INFO] LM loaded " << count << " compressed Bigram transitions. (Skipped " << oov_count << " OOV entries)" << std::endl;
    std::cout << "[INFO] LM built forward topology for " << forward_edges.size() << " unique context states." << std::endl;
}

double BigramLM::get_transition_log_p(int state1, int state2, double unigram_log_p) const {
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

const std::vector<std::pair<int, double>>* BigramLM::get_forward_transitions(int state1) const {
    auto it = forward_edges.find(state1);
    if (it != forward_edges.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::string BigramLM::get_word_by_state(int state) const {
    auto it = state_to_word.find(state);
    if (it != state_to_word.end()) {
        return it->second;
    }
    return ""; 
}