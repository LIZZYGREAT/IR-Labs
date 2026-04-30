#pragma once
#include "double_array_trie.h"
#include <string>
#include <unordered_map>
#include <cstdint>

class BigramLM {
private:
    std::unordered_map<uint64_t, double> transitions;
    double backoff_penalty;

    inline uint64_t pack_key(int state1, int state2) const {
        return (static_cast<uint64_t>(static_cast<uint32_t>(state1)) << 32) | static_cast<uint32_t>(state2);
    }

public:
    BigramLM(const std::string& tsv_filepath, const DoubleArrayTrie& dat, double backoff_penalty_val = -5.0);

    double get_transition_log_p(int state1, int state2, double unigram_log_p) const;
};