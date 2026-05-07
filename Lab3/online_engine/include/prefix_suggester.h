#ifndef PREFIX_SUGGESTER_H
#define PREFIX_SUGGESTER_H

#include <string>
#include <vector>
#include <queue>
#include "double_array_trie.h"
#include "bigram_lm.h"
#include "trie_dp_search.h"

struct Suggestion {
    std::string full_text; 
    double score;          
    
    bool operator<(const Suggestion& other) const {
        return score > other.score; 
    }
};

class PrefixSuggester {
private:
    const DoubleArrayTrie& dat;
    const BigramLM& lm;
    const TrieDPSearcher& searcher; 
    const int max_dfs_steps;
    
    SearchParams strict_config;

    void dfs_subtree(
        int current_state, 
        const std::string& current_word, 
        const std::string& context_str, 
        int context_state,
        std::priority_queue<Suggestion>& min_heap, 
        size_t k,
        int& steps_taken
    ) const;

public:
    PrefixSuggester(const DoubleArrayTrie& dat_ref, const BigramLM& lm_ref, const TrieDPSearcher& searcher_ref, int max_steps = 2000);

    std::vector<Suggestion> suggest(const std::string& raw_input, size_t k) const;
};

#endif