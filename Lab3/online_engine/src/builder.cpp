#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cctype>

// 引入解耦后的基础构建组件
#include "../include/dat_builder.h"

int main() {
    std::string input_file = "../data/processed/unigram_priors.tsv";
    std::string output_dir = "../data/index/words"; 
    // 新增：权威静态词库路径 (纯文本词表)
    std::string dic_file = "../data/raw/en_US.dic"; 

    ArenaTrie trie;
    std::unordered_set<std::string> valid_words;

    // ===================================================================
    // PHASE 0: 加载权威标准词库白名单 (Hunspell Dictionary Loading)
    // ===================================================================
    std::ifstream dic_stream(dic_file);
    if (dic_stream.is_open()) {
        std::cout << "[INFO] Loading Hunspell standard dictionary: " << dic_file << std::endl;
        std::string dic_line;
        while (std::getline(dic_stream, dic_line)) {
            if (dic_line.empty()) continue;

            // 提取斜杠 '/' 前的纯单词部分 (忽略 Hunspell 的词缀衍生规则标记)
            size_t slash_pos = dic_line.find('/');
            std::string word = (slash_pos != std::string::npos) ? dic_line.substr(0, slash_pos) : dic_line;
            
            // 清洗回车换行符及两端空白字符
            word.erase(word.find_last_not_of(" \n\r\t") + 1);
            word.erase(0, word.find_first_not_of(" \n\r\t"));
            
            // 统一转换为小写以对齐 TSV 语料格式
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            
            if (!word.empty()) {
                valid_words.insert(word);
            }
        }
        dic_stream.close();
        std::cout << "[SUCCESS] Loaded " << valid_words.size() << " standard root words." << std::endl;
    } else {
        std::cout << "[WARNING] Standard dictionary not found at " << dic_file << ". Proceeding with frequency truncation only." << std::endl;
    }

    // ===================================================================
    // PHASE 1: 解析 TSV 并执行双重清洗过滤 (Dual-Filtering Parsing)
    // ===================================================================
    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "[ERROR] Cannot open unigram input file: " << input_file << std::endl;
        return 1;
    }

    std::cout << "[PHASE 1] Parsing TSV for Word Index with Dual-Filtering..." << std::endl;
    std::string line;
    std::getline(infile, line); // 跳过表头

    int line_count = 0;
    int skipped_count = 0;
    // 针对不在 Hunspell 词库中的集外词（OOV，如 stm32, nemu），强制要求频次达到 50 以上方可准入
    const long long MIN_FREQ_FOR_OOV = 50; 

    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        size_t first_tab = line.find('\t');
        if (first_tab == std::string::npos) continue;

        size_t second_tab = line.find('\t', first_tab + 1);
        if (second_tab != std::string::npos) {
            std::string ngram = line.substr(0, first_tab);
            try {
                // 精准提取中间列的 frequency 与最后一列的 log_p_i
                long long freq = std::stoll(line.substr(first_tab + 1, second_tab - first_tab - 1));
                double log_p_i = std::stod(line.substr(second_tab + 1));

                // --- 核心清洗过滤防线 ---
                // 条件1：单词直接命中标准词典白名单
                // 条件2：虽为集外词，但出现频次极高，判定为领域专有名词而非偶发错词
                bool in_dict = (valid_words.find(ngram) != valid_words.end());
                
                if (in_dict || freq >= MIN_FREQ_FOR_OOV) {
                    trie.insert(ngram, log_p_i);
                    line_count++;
                } else {
                    skipped_count++; // 低频噪声（如 whta，频次仅3）在此处被精准拦截抛弃
                }
            } catch (...) { continue; }
        }
    }
    infile.close();
    std::cout << "[SUCCESS] Loaded " << line_count << " clean words into ArenaTrie. (Filtered out " << skipped_count << " noise tokens)" << std::endl;
    
    // ===================================================================
    // PHASE 2: 权重传播 (Subtree Weight Propagation)
    // ===================================================================
    std::cout << "[PHASE 2] Propagating Max Subtree Weights..." << std::endl;
    trie.compute_max_weights(trie.root);

    // ===================================================================
    // PHASE 3: 映射至双数组结构 (Double-Array Mapping)
    // ===================================================================
    std::cout << "[PHASE 3] Mapping to Double-Array..." << std::endl;
    DATBuilder builder(5000000);
    builder.build_from_arena(trie);

    // ===================================================================
    // PHASE 4: 持久化刷盘 (Persistent Storage)
    // ===================================================================
    std::cout << "[PHASE 4] Persistent Storage to " << output_dir << "..." << std::endl;
    builder.export_mmap(output_dir);

    return 0;
}