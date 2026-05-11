#ifndef TRIE_DP_SEARCH_H
#define TRIE_DP_SEARCH_H

#include "double_array_trie.h"
#include "search_config.h" 
#include <vector>
#include <string>
#include <queue>
#include <algorithm>

#define MAX_QUERY_LEN 64

struct Candidate {
    std::string word;
    double edit_distance;
    double log_p_i;
    int state_id;
    double penalty_score; 

    bool operator<(const Candidate& other) const {
        return penalty_score > other.penalty_score; 
    }
};

class TrieDPSearcher {
private:
    const DoubleArrayTrie& dat;
    double sub_cost_table[128][128];
    const std::string valid_charset = "abcdefghijklmnopqrstuvwxyz";

    void init_cost_table();
    
    void dfs(
        int current_state, 
        const std::string& current_prefix, 
        const std::string& query, 
        const double* prev_row, 
        const double* prev_prev_row, 
        char prev_c,
        int query_len,
        double& dynamic_threshold, 
        std::priority_queue<Candidate>& pq, 
        size_t k,
        const SearchParams& config
    ) const;

    inline double get_sub_cost(char c1, char c2) const {
        return sub_cost_table[static_cast<int>(c1)][static_cast<int>(c2)];
    }
    
    inline double get_del_cost(char c) const { return 1.0; }
    inline double get_ins_cost(char c) const { return 1.0; }

    inline double get_dynamic_edit_cost(int depth, bool is_sub) const;


public:
    TrieDPSearcher(const DoubleArrayTrie& dat_instance);
    std::vector<Candidate> search(const std::string& query, const SearchParams& params, size_t k) const;
};

#endif