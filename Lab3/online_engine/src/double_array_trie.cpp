#include "../include/double_array_trie.h"
#include <stdexcept>
#include <iostream>

DoubleArrayTrie::DoubleArrayTrie(const std::string& index_dir) {
    std::string base_path = index_dir + "/base.bin";
    std::string check_path = index_dir + "/check.bin";
    std::string weights_path = index_dir + "/weights.bin";

    try {
        base_loader = std::make_unique<MMapLoader<int>>(base_path);
        check_loader = std::make_unique<MMapLoader<int>>(check_path);
        weights_loader = std::make_unique<MMapLoader<double>>(weights_path);

        base = base_loader->data();
        check = check_loader->data();
        weights = weights_loader->data();

        array_size = base_loader->size();

        // Sanity check: ensure all three arrays have identical length
        if (check_loader->size() != array_size || weights_loader->size() != array_size) {
            throw std::runtime_error("DAT binary file lengths mismatch, index may be corrupted!");
        }
        
        std::cout << "DoubleArrayTrie engine initialized, total states: " << array_size << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("DAT loading failed: ") + e.what());
    }
}

int DoubleArrayTrie::get_root_state() const {
    return 1; // During offline construction, state 1 is designated as Root
}

int DoubleArrayTrie::get_next_state(int current_state, char c) const {
    // 修复符号警告：显式转换为 int
    if (current_state <= 0 || current_state >= static_cast<int>(array_size)) {
        return -1;
    }

    int char_code = get_char_code(c);
    int next_state = base[current_state] + char_code;

    // 修复符号警告：显式转换为 int
    if (next_state <= 0 || next_state >= static_cast<int>(array_size)) {
        return -1;
    }

    // 核心转移校验：如果目标状态的父节点确实是当前状态，则转移合法
    if (check[next_state] == current_state) {
        return next_state;
    }

    return -1; // 哈希冲突或该分支根本不存在
}

bool DoubleArrayTrie::is_word_end(int state) const {
    // 修复符号警告：显式转换为 int
    if (state <= 0 || state >= static_cast<int>(array_size)) return false;
    // 逻辑锚点：离线构建时，非单词结尾的权重初始化为 0.0。
    // 由于真实的先验概率对数 log P(I) 必然小于 0（负数），据此判断是否为词尾。
    return weights[state] < 0.0;
}

double DoubleArrayTrie::get_weight(int state) const {
    // 修复符号警告：显式转换为 int
    if (state <= 0 || state >= static_cast<int>(array_size)) return 0.0;
    return weights[state];
}

bool DoubleArrayTrie::exact_match(const std::string& word) const {
    int current_state = get_root_state();
    for (char c : word) {
        current_state = get_next_state(current_state, c);
        if (current_state == -1) {
            return false;
        }
    }
    return is_word_end(current_state);
}

int DoubleArrayTrie::get_word_state(const std::string& word) const {
    int current_state = get_root_state();
    for (char c : word) {
        current_state = get_next_state(current_state, c);
        if (current_state == -1) return -1;
    }
    return is_word_end(current_state) ? current_state : -1;
}