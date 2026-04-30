#pragma once
#include "mmap_loader.h"
#include <memory>
#include <string>

class DoubleArrayTrie {
private:
    std::unique_ptr<MMapLoader<int>> base_loader;
    std::unique_ptr<MMapLoader<int>> check_loader;
    std::unique_ptr<MMapLoader<double>> weights_loader;

    const int* base;
    const int* check;
    const double* weights;
    size_t array_size;

    inline int get_char_code(char c) const {
        return static_cast<int>(c);
    }

public:
    DoubleArrayTrie(const std::string& index_dir);

    int get_root_state() const;
    int get_next_state(int current_state, char c) const;
    bool is_word_end(int state) const;
    double get_weight(int state) const;
    bool exact_match(const std::string& word) const;
    int get_word_state(const std::string& word) const;
    
};