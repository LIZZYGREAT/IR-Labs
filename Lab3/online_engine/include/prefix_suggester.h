#ifndef PREFIX_SUGGESTER_H
#define PREFIX_SUGGESTER_H

#include <string>
#include <vector>
#include <queue>
#include "double_array_trie.h"
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
    const DoubleArrayTrie& query_dat;  // 负责整句召回的 DAT
    const DoubleArrayTrie& word_dat;   // 负责判定单词正确性的 DAT
    const TrieDPSearcher& searcher;    // 负责拼写纠错的搜索器
    
    int max_dfs_steps;
    double drop_penalty;               // LCP-Fallback 时，每丢弃一个失配字符的惩罚分数
    
    SearchParams strict_config;        // 针对轻微拼写错或常规校验的严格配置
    SearchParams force_config;         // 针对词典中绝对不存在错词的强制替换配置

    /**
     * @brief 带有失配惩罚的 A* 启发式整句搜索
     * 此时的 charset 必须包含空格
     */
    void dfs_subtree(
        int current_state, 
        std::string& current_prefix_buffer, 
        std::priority_queue<Suggestion>& min_heap, 
        size_t k,
        int drop_count,
        int& steps_taken
    ) const;

    /**
     * @brief 前缀清洗协议：对已完成输入的单词进行逐一纠错校验
     */
    std::string clean_prefix(const std::string& raw_input) const;

public:
    PrefixSuggester(
        const DoubleArrayTrie& query_dat_ref, 
        const DoubleArrayTrie& word_dat_ref,
        const TrieDPSearcher& searcher_ref, 
        int max_steps = 5000, 
        double penalty = 2.0
    );

    std::vector<Suggestion> suggest(const std::string& raw_input, size_t k) const;
};

#endif