#pragma once
#include "mmap_loader.h"
#include <memory>
#include <string>

/**
 * @brief 启发式增强型双数组 Trie
 * 每一个 state 存储该分支下的最大潜在权重，用于 A* 剪枝。
 */
class DoubleArrayTrie {
private:
    std::unique_ptr<MMapLoader<int>> base_loader;
    std::unique_ptr<MMapLoader<int>> check_loader;
    std::unique_ptr<MMapLoader<double>> weights_loader;

    const int* base;
    const int* check;
    const double* weights; // 存储子树最大权重 MaxSubtreeWeight
    size_t array_size;

    inline int get_char_code(char c) const {
        return static_cast<unsigned char>(c);
    }

public:
    DoubleArrayTrie(const std::string& index_dir);

    int get_root_state() const;
    int get_next_state(int current_state, char c) const;
    
    // 状态属性查询
    bool is_word_end(int state) const;
    
    /**
     * @brief 获取当前状态及其子树的最大 log P 权重
     * 用于启发式搜索：Score_upper_bound = get_max_weight(s) - edit_dist * lambda
     */
    double get_max_weight(int state) const;

    int get_word_state(const std::string& word) const;
};