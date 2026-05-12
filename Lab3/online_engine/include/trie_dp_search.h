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

    // 修复：重载为适配 std::priority_queue 的堆排序比较逻辑。
    // 使得 penalty_score 较大（即综合质量较劣）的元素置于 top()，
    // 确保当优先队列超出设定容量 k 时，优先执行 pop() 淘汰最劣解。
    bool operator<(const Candidate& other) const {
        return penalty_score < other.penalty_score; 
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

    // --- 新增：辅助校验与打分机制声明 (辅助前置斩杀与多维信道建模) ---
    bool passes_feasibility_gating(const std::string& cand_word, const std::string& query, const SearchParams& config) const;
    double calculate_lcs_ratio(const std::string& s1, const std::string& s2) const;
    double calculate_char_overlap_ratio(const std::string& cand_word, const std::string& query) const;
    double calculate_composite_penalty(const std::string& cand_word, const std::string& query, double weighted_edit_dist, double prior_log_p, const SearchParams& config) const;

public:
    TrieDPSearcher(const DoubleArrayTrie& dat_instance);
    std::vector<Candidate> search(const std::string& query, const SearchParams& params, size_t k) const;
};

#endif