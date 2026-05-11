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

        if (check_loader->size() != array_size || weights_loader->size() != array_size) {
            throw std::runtime_error("DAT binary file lengths mismatch!");
        }
        
        std::cout << "[INFO] DAT Engine initialized with Heuristic Weights. States: " << array_size << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("DAT loading failed: ") + e.what());
    }
}

int DoubleArrayTrie::get_root_state() const { return 1; }

int DoubleArrayTrie::get_next_state(int current_state, char c) const {
    // 此时需处理 base 值为负数的情况（word_end 标记）
    int current_base = base[current_state];
    if (current_base < 0) current_base = -current_base; // 解包实际 base 指针

    if (current_state <= 0 || current_state >= static_cast<int>(array_size)) return -1;

    int next_state = current_base + get_char_code(c);

    if (next_state <= 0 || next_state >= static_cast<int>(array_size)) return -1;

    if (check[next_state] == current_state) return next_state;
    return -1;
}

bool DoubleArrayTrie::is_word_end(int state) const {
    if (state <= 0 || state >= static_cast<int>(array_size)) return false;
    // 约定：在 Builder 中，如果该状态是一个合法单词的结尾，则将其 base 设为负数
    return base[state] < 0;
}

double DoubleArrayTrie::get_max_weight(int state) const {
    if (state <= 0 || state >= static_cast<int>(array_size)) return -30.0; // 极小值惩罚
    return weights[state];
}

int DoubleArrayTrie::get_word_state(const std::string& word) const {
    int current_state = get_root_state();
    for (char c : word) {
        current_state = get_next_state(current_state, c);
        if (current_state == -1) return -1;
    }
    return is_word_end(current_state) ? current_state : -1;
}