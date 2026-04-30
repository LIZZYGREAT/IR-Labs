#pragma once
#include "trie_dp_search.h"
#include "bigram_lm.h"
#include <vector>
#include <string>

struct DecodedSentence {
    std::string text;
    double final_score;
};

class ViterbiDecoder {
private:
    const BigramLM& lm;
    double lambda;          
    size_t beam_width;      
    
    // --- 动态零惩罚锚点参数 (Dynamic Zero-Edit Bonus Parameters) ---
    double base_zero_bonus; // 基础奖励
    double rare_threshold;  // 罕见词阈值 (log P_i 低于此值的词，即使精确匹配也不给奖励)
    double length_weight;   // 长度增益系数 (每多一个字母带来的额外置信度)
    double freq_weight;     // 频次增益系数 (高于罕见阈值部分的概率权重映射)

    // 内部联函数：计算动态精确匹配奖励
    inline double calculate_dynamic_bonus(const Candidate& cand) const {
        if (cand.edit_distance > 0.0) return 0.0;
        if (cand.log_p_i < rare_threshold) return 0.0; // 罕见词不予原词保护

        double freq_factor = cand.log_p_i - rare_threshold; 
        double len_factor = static_cast<double>(cand.word.length());
        
        return base_zero_bonus + (length_weight * len_factor) + (freq_weight * freq_factor);
    }

public:
    // 构造函数提供默认参数，均经过量纲对齐的初步预设，实际项目中可通过网格搜索(Grid Search)微调
    ViterbiDecoder(
        const BigramLM& language_model, 
        double lambda_val = 2.5, 
        size_t beam_width_val = 5, 
        double base_bonus = 5.0,
        double rare_thresh = -12.0, 
        double len_weight = 0.5,
        double f_weight = 0.2
    );

    std::vector<DecodedSentence> decode(const std::vector<std::vector<Candidate>>& token_candidates) const;
};