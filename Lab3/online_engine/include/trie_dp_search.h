#pragma once
#include "double_array_trie.h"
#include <string>
#include <vector>
#include <queue>

struct Candidate {
    std::string word;
    double edit_distance;
    double log_p_i; 
    int state_id;

    bool operator<(const Candidate& other) const {
        return edit_distance < other.edit_distance;
    }
};

class TrieDPSearcher {
private:
    const DoubleArrayTrie& dat; 
    
    static constexpr int MAX_QUERY_LEN = 128; 
    const std::string valid_charset = "abcdefghijklmnopqrstuvwxyz";
    double sub_cost_table[128][128];
    
    void init_cost_table();

    inline double get_sub_cost(char dict_char, char query_char) const {
        return sub_cost_table[static_cast<int>(dict_char)][static_cast<int>(query_char)];
    }
    inline double get_del_cost(char dict_char) const;
    inline double get_ins_cost(char query_char) const;

    void dfs(
        int current_state, 
        const std::string& current_prefix, 
        const std::string& query, 
        const double* prev_row, 
        int query_len,
        double& dynamic_threshold, 
        std::priority_queue<Candidate>& pq, 
        size_t k
    ) const;

public:
    TrieDPSearcher(const DoubleArrayTrie& dat_instance);
    std::vector<Candidate> search(const std::string& query, double initial_threshold = 2.5, size_t k = 10) const;
};