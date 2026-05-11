#include <iostream>
#include <fstream>
#include <string>

// 引入解耦后的基础构建组件
#include "../include/dat_builder.h"

int main() {
    std::string input_file = "../data/processed/unigram_priors.tsv";
    
    std::string output_dir = "../data/index/words"; 

    ArenaTrie trie;
    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "[ERROR] Cannot open unigram input file." << std::endl;
        return 1;
    }

    std::cout << "[PHASE 1] Parsing TSV for Word Index..." << std::endl;
    std::string line;
    std::getline(infile, line); // 跳过表头

    int line_count = 0;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        size_t first_tab = line.find('\t');
        if (first_tab == std::string::npos) continue;

        size_t second_tab = line.find('\t', first_tab + 1);
        if (second_tab != std::string::npos) {
            std::string ngram = line.substr(0, first_tab);
            try {
                double log_p_i = std::stod(line.substr(second_tab + 1));
                trie.insert(ngram, log_p_i);
                line_count++;
            } catch (...) { continue; }
        }
    }
    infile.close();
    std::cout << "[SUCCESS] Loaded " << line_count << " words into ArenaTrie." << std::endl;
    
    std::cout << "[PHASE 2] Propagating Max Subtree Weights..." << std::endl;
    trie.compute_max_weights(trie.root);

    std::cout << "[PHASE 3] Mapping to Double-Array..." << std::endl;
    DATBuilder builder(5000000);
    builder.build_from_arena(trie);

    std::cout << "[PHASE 4] Persistent Storage..." << std::endl;
    builder.export_mmap(output_dir);

    return 0;
}