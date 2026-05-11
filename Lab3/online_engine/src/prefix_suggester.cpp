#include "../include/prefix_suggester.h"
#include <sstream>
#include <algorithm>
#include <queue>

PrefixSuggester::PrefixSuggester(
    const DoubleArrayTrie& query_dat_ref, 
    const DoubleArrayTrie& word_dat_ref,
    const TrieDPSearcher& searcher_ref, 
    int max_steps,
    double penalty
) : query_dat(query_dat_ref), 
    word_dat(word_dat_ref), 
    searcher(searcher_ref), 
    max_dfs_steps(max_steps),
    drop_penalty(penalty) {
    
    // 补全前置纠错时的配置，保持严格以避免过度纠错改变原意
    strict_config.base_threshold = 1.5;
    strict_config.fallback_limit = 1.5;
    strict_config.max_error_ratio = 0.30; 
    strict_config.heuristic_lambda = 3.0;
    strict_config.local_beam_width = 5; 
}

std::string PrefixSuggester::clean_prefix(const std::string& raw_input) const {
    if (raw_input.empty()) return "";

    std::string lower_input = raw_input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

    std::vector<std::string> tokens;
    std::istringstream iss(lower_input);
    std::string token;
    while (iss >> token) tokens.push_back(token);

    if (tokens.empty()) return "";

    bool ends_with_space = (raw_input.back() == ' ');
    std::string cleaned_str = "";

    // 遍历除最后一个 token 外的所有“已输入完毕”的单词
    size_t complete_words_count = ends_with_space ? tokens.size() : tokens.size() - 1;
    
    for (size_t i = 0; i < complete_words_count; ++i) {
        // 如果该单词在基础词典中不存在，强制纠错
        if (word_dat.get_word_state(tokens[i]) == -1) {
            auto corrections = searcher.search(tokens[i], strict_config, 1);
            if (!corrections.empty()) {
                cleaned_str += corrections[0].word + " ";
            } else {
                cleaned_str += tokens[i] + " "; // 纠错失败，保留原样
            }
        } else {
            cleaned_str += tokens[i] + " ";
        }
    }

    // 处理活跃前缀（还在输入中，不进行纠错）
    if (!ends_with_space && tokens.size() > 0) {
        cleaned_str += tokens.back();
    }

    return cleaned_str;
}

void PrefixSuggester::dfs_subtree(
    int current_state, 
    std::string& current_prefix_buffer, 
    std::priority_queue<Suggestion>& min_heap, 
    size_t k,
    int drop_count,
    int& steps_taken
) const {
    if (steps_taken++ > max_dfs_steps) return;

    // --- A* 启发式剪枝结合失配惩罚 ---
    // 理论最高分 = 该子树存放的历史最高权重 - 丢弃字符的惩罚
    double max_potential_score = query_dat.get_max_weight(current_state) - (drop_penalty * drop_count);
    
    if (min_heap.size() == k && max_potential_score < min_heap.top().score) {
        return; 
    }

    // 触达整句叶子节点
    if (query_dat.is_word_end(current_state)) {
        // 这里的 get_max_weight(leaf) 提取出的就是我们在 query_builder 中写入的内禀流畅度得分 W_base
        double final_score = query_dat.get_max_weight(current_state) - (drop_penalty * drop_count);
        min_heap.push({current_prefix_buffer, final_score});
        if (min_heap.size() > k) min_heap.pop();
    }

    // 致命细节修复：遍历字符集必须包含空格 ' '，否则无法跨越单词边界
    const std::string query_charset = "abcdefghijklmnopqrstuvwxyz ";
    
    for (char c : query_charset) {
        int next_state = query_dat.get_next_state(current_state, c);
        if (next_state != -1) {
            current_prefix_buffer.push_back(c);
            dfs_subtree(next_state, current_prefix_buffer, min_heap, k, drop_count, steps_taken);
            current_prefix_buffer.pop_back(); 
        }
    }
}

std::vector<Suggestion> PrefixSuggester::suggest(const std::string& raw_input, size_t k) const {
    if (raw_input.empty()) return {};

    // 1. 纠错清洗前缀 (Prefix Cleaning)
    std::string safe_prefix = clean_prefix(raw_input);
    if (safe_prefix.empty()) return {};

    // 2. 最长公共前缀下探 (LCP Traversal)
    int current_state = query_dat.get_root_state();
    int matched_len = 0;

    for (char c : safe_prefix) {
        int next_state = query_dat.get_next_state(current_state, c);
        if (next_state == -1) {
            break; // 触发物理断崖
        }
        current_state = next_state;
        matched_len++;
    }

    // 3. 计算断崖回退惩罚 (LCP Fallback Calculation)
    int drop_count = safe_prefix.length() - matched_len;
    std::string matched_str = safe_prefix.substr(0, matched_len);

    // 4. DFS 召回
    std::priority_queue<Suggestion> min_heap;
    int steps_taken = 0;
    std::string buffer = matched_str; 
    
    dfs_subtree(current_state, buffer, min_heap, k, drop_count, steps_taken);

    // 5. 排序输出
    std::vector<Suggestion> results;
    while (!min_heap.empty()) {
        results.push_back(min_heap.top());
        min_heap.pop();
    }
    std::reverse(results.begin(), results.end());
    return results;
}