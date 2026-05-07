#ifndef BIGRAM_LM_H
#define BIGRAM_LM_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "double_array_trie.h"

class BigramLM {
private:
    std::unordered_map<uint64_t, double> transitions;
    
    // Key: state1, Value: 所有的 {state2, transition_log_p}
    std::unordered_map<int, std::vector<std::pair<int, double>>> forward_edges;
    
    std::unordered_map<int, std::string> state_to_word;

    double backoff_penalty;

    inline uint64_t pack_key(int s1, int s2) const {
        return (static_cast<uint64_t>(s1) << 32) | static_cast<uint32_t>(s2);
    }

public:
    BigramLM(const std::string& tsv_filepath, const DoubleArrayTrie& dat, double backoff_penalty_val = -5.0);

    double get_transition_log_p(int state1, int state2, double unigram_log_p) const;
    
    const std::vector<std::pair<int, double>>* get_forward_transitions(int state1) const;

    std::string get_word_by_state(int state) const;
};

#endif // BIGRAM_LM_H