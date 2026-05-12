#ifndef SEARCH_CONFIG_H
#define SEARCH_CONFIG_H

struct SearchParams {
    // --- 原有基础检索与配置参数 (保持向下兼容) ---
    double base_threshold = 2.5;
    double max_error_ratio = 0.45;
    double length_compensation = 0.2;
    double dist_weight = 2.0;
    double fallback_limit = 2.0;
    double confidence_fuse = -15.0;
    
    // --- 启发式与束搜索参数 ---
    double heuristic_lambda = 3.0; 
    int local_beam_width = 10;      

    // 1. 前置门控斩杀机制参数 (Feasibility Gating Parameters)
    double max_len_diff_ratio = 0.4;       // 候选词与输入词长度差异的最大允许比率 (\theta_{len})
    double min_char_overlap_ratio = 0.6;   // 输入串字符集在候选词中的最小重合度覆盖比率底线
    double max_edit_dist_ratio = 0.5;      // 动态相对编辑距离上限乘积系数 (\alpha)

    // 2. 多维复合信道打分权重 (Multi-Dimensional Channel Scoring Weights)
    double weight_edit_dist = 1.0;         // 物理加权编辑距离的惩罚权重 (w_1)
    double weight_len_penalty = 0.5;       // 长度差异非线性惩罚权重 (w_2)
    double weight_lcs_penalty = 0.8;       // 最长公共子序列缺失度惩罚权重 (w_3)

    // 3. 先验概率受限温和放大参数 (Bounded Prior Boosting Parameters)
    double prior_dampening_gamma = 0.6;    // 词频对数映射的阻尼压缩指数因子 (\gamma)
    double prior_base_frequency = 1000.0;  // 相对频次计算的基准平滑分母 (Freq_{base})
    double max_prior_bonus_cap = 1.2;      // 极限先验奖励分值上限，确保绝对不超过单字符物理修改代价
};

#endif