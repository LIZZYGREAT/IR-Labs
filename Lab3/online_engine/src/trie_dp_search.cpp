#include "../include/trie_dp_search.h"
#include <algorithm>
#include <cmath>
#include <vector>

/**
 * @brief 启发式搜索候选结构
 */
struct LayerCandidate {
    char c;
    int state;
    double row[MAX_QUERY_LEN + 1];
    double heuristic_score;

    // 降序排列：启发式得分越高越靠前
    bool operator>(const LayerCandidate& other) const {
        return heuristic_score > other.heuristic_score;
    }
};

TrieDPSearcher::TrieDPSearcher(const DoubleArrayTrie& dat_instance) 
    : dat(dat_instance) {
    init_cost_table(); 
}

void TrieDPSearcher::init_cost_table() {
    // 默认高惩罚初始化
    for (int i = 0; i < 128; ++i) {
        for (int j = 0; j < 128; ++j) {
            sub_cost_table[i][j] = 2.0; 
        }
    }

    struct Coord { int r; int c; };
    Coord kb[128];
    for (int i = 0; i < 128; ++i) kb[i] = {-1, -1};

    // 键盘物理布局映射
    kb['q']={0,0}; kb['w']={0,1}; kb['e']={0,2}; kb['r']={0,3}; kb['t']={0,4}; kb['y']={0,5}; kb['u']={0,6}; kb['i']={0,7}; kb['o']={0,8}; kb['p']={0,9};
    kb['a']={1,0}; kb['s']={1,1}; kb['d']={1,2}; kb['f']={1,3}; kb['g']={1,4}; kb['h']={1,5}; kb['j']={1,6}; kb['k']={1,7}; kb['l']={1,8};
    kb['z']={2,1}; kb['x']={2,2}; kb['c']={2,3}; kb['v']={2,4}; kb['b']={2,5}; kb['n']={2,6}; kb['m']={2,7};

    for (char c1 = 'a'; c1 <= 'z'; ++c1) {
        for (char c2 = 'a'; c2 <= 'z'; ++c2) {
            if (c1 == c2) {
                sub_cost_table[(int)c1][(int)c2] = 0.0;
            } else {
                Coord p1 = kb[(int)c1], p2 = kb[(int)c2];
                if (p1.r == -1 || p2.r == -1) continue;
                int dist = std::abs(p1.r - p2.r) + std::abs(p1.c - p2.c);
                sub_cost_table[(int)c1][(int)c2] = std::min(0.6 + (0.2 * dist), 2.0);
            }
        }
    }
}

/**
 * @brief 深度感知代价控制
 * 针对短前缀 (L<=3) 显著拉高增删代价，模拟“高信息密度，低容错”的物理规律
 */
inline double TrieDPSearcher::get_dynamic_edit_cost(int depth, bool is_sub) const {
    if (is_sub) return 1.0; 
    return (depth <= 3) ? 2.5 : 1.0; 
}

// ============================================================================
// 新增核心功能实现：前置斩杀校验与多维信道特征测算
// ============================================================================

/**
 * @brief 测算候选词与查询词之间的字符集覆盖重合度占比
 */
double TrieDPSearcher::calculate_char_overlap_ratio(const std::string& cand_word, const std::string& query) const {
    if (query.empty()) return 0.0;
    
    bool q_chars[256] = {false};
    int unique_q_count = 0;
    
    for (char c : query) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!q_chars[uc]) {
            q_chars[uc] = true;
            unique_q_count++;
        }
    }
    
    if (unique_q_count == 0) return 0.0;
    
    int matched_count = 0;
    for (int i = 0; i < 256; ++i) {
        if (q_chars[i]) {
            if (cand_word.find(static_cast<char>(i)) != std::string::npos) {
                matched_count++;
            }
        }
    }
    
    return static_cast<double>(matched_count) / unique_q_count;
}

/**
 * @brief 执行前置门控斩杀审计，若触犯任一物理红线直接予以剪枝
 */
bool TrieDPSearcher::passes_feasibility_gating(const std::string& cand_word, const std::string& query, const SearchParams& config) const {
    double len_c = cand_word.length();
    double len_q = query.length();
    if (len_c == 0 || len_q == 0) return false;
    
    // 红线一：动态相对长度约束审计
    double max_len = std::max(len_c, len_q);
    if (std::abs(len_c - len_q) / max_len > config.max_len_diff_ratio) {
        return false;
    }
    
    // 红线二：针对较长输入的字符集底层重合度底线审计
    if (len_q >= 3) {
        double overlap = calculate_char_overlap_ratio(cand_word, query);
        if (overlap < config.min_char_overlap_ratio) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 动态规划计算最长公共子序列占比 (LCS Ratio)，评估词根骨架完整度
 */
double TrieDPSearcher::calculate_lcs_ratio(const std::string& s1, const std::string& s2) const {
    int len1 = s1.length();
    int len2 = s2.length();
    if (len1 == 0 || len2 == 0) return 0.0;
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1, 0));
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }
    
    int lcs_len = dp[len1][len2];
    // 相对较长字符串进行归一化惩罚，压制碎片化匹配
    return static_cast<double>(lcs_len) / std::max(len1, len2);
}

/**
 * @brief 测算综合信道惩罚指标 (融入加权距离、长度差异、LCS骨架惩罚及受限先验奖励)
 */
double TrieDPSearcher::calculate_composite_penalty(
    const std::string& cand_word, 
    const std::string& query, 
    double weighted_edit_dist, 
    double prior_log_p, 
    const SearchParams& config
) const {
    double lcs_ratio = calculate_lcs_ratio(cand_word, query);
    double len_penalty = std::abs(static_cast<double>(cand_word.length()) - static_cast<double>(query.length()));
    
    // 基于对数概率平滑反推估算绝对物理频次
    double estimated_freq = std::exp(prior_log_p) * 5000000.0;
    
    // 先验概率受限温和放大映射：具备平滑饱和特性
    double prior_bonus = std::log(1.0 + std::max(0.0, estimated_freq) / config.prior_base_frequency) * config.prior_dampening_gamma;
    // 强制设立量纲天花板，保障绝对不超过单字符的基础编辑修改代价
    prior_bonus = std::min(prior_bonus, config.max_prior_bonus_cap);
    
    // 最终信道综合成本打分融合
    double composite_penalty = (config.weight_edit_dist * weighted_edit_dist) +
                               (config.weight_len_penalty * len_penalty) +
                               (config.weight_lcs_penalty * (1.0 - lcs_ratio)) - 
                               prior_bonus;
                               
    return composite_penalty;
}

// ============================================================================
// 主干检索逻辑
// ============================================================================

std::vector<Candidate> TrieDPSearcher::search(const std::string& query, const SearchParams& params, size_t k) const {
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    int query_len = lower_query.length(); 
    if (query_len == 0 || query_len > MAX_QUERY_LEN - 1) return {};

    double root_row[MAX_QUERY_LEN + 1]; 
    root_row[0] = 0.0;
    for (int j = 1; j <= query_len; ++j) {
        root_row[j] = root_row[j - 1] + 1.0;
    }

    std::priority_queue<Candidate> pq;
    double current_dynamic_limit = params.base_threshold;

    // 展开第一层节点
    for (char c : valid_charset) {
        int next_state = dat.get_next_state(dat.get_root_state(), c);
        if (next_state == -1) continue;

        double curr_row[MAX_QUERY_LEN + 1];
        double edit_cost = get_dynamic_edit_cost(1, false);
        curr_row[0] = root_row[0] + edit_cost;

        for (int j = 1; j <= query_len; ++j) {
            double ins = curr_row[j - 1] + 1.0;
            double del = root_row[j] + edit_cost;
            double sub = root_row[j - 1] + get_sub_cost(c, query[j - 1]);
            curr_row[j] = std::min({ins, del, sub});
        }
        std::string prefix(1, c);
        dfs(next_state, prefix, query, curr_row, root_row, '\0', query_len, current_dynamic_limit, pq, k, params);
    }

    std::vector<Candidate> results;
    while (!pq.empty()) {
        results.push_back(pq.top());
        pq.pop();
    }
    // 优先队列基于 penalty_score 构建大顶堆淘汰劣解，在此反转呈现最优至最劣排序
    std::reverse(results.begin(), results.end());

    // 核心强化：绝对原词保送逻辑。只要原词合法存在于 DAT 中且满足门控约束，优先锁定首位
    int exact_state = dat.get_word_state(lower_query);
    if (exact_state != -1) {
        double exact_prior = dat.get_max_weight(exact_state);
        double exact_penalty = calculate_composite_penalty(lower_query, lower_query, 0.0, exact_prior, params);
        
        // 赋予极具统治力的置信度偏置以锁定榜首展示
        Candidate exact_cand = {lower_query, 0.0, exact_prior, exact_state, exact_penalty - 100.0};

        auto it = std::find_if(results.begin(), results.end(), [&](const Candidate& c) {
            return c.word == lower_query;
        });

        if (it != results.end()) {
            results.erase(it);
        }
        results.insert(results.begin(), exact_cand);
        if (results.size() > k) results.pop_back();
    }

    return results;
}

void TrieDPSearcher::dfs(
    int current_state, 
    const std::string& current_prefix, 
    const std::string& query, 
    const double* prev_row, 
    const double* prev_prev_row,
    char prev_c,                 
    int query_len,
    double& dynamic_threshold, 
    std::priority_queue<Candidate>& pq, 
    size_t k,
    const SearchParams& config
) const {
    int current_depth = current_prefix.length();
    
    // ==========================================
    // 1. A* 启发式全局剪枝
    // ==========================================
    double max_weight = dat.get_max_weight(current_state);
    double min_dist_in_prev = prev_row[0];
    for (int j = 1; j <= query_len; ++j) {
        min_dist_in_prev = std::min(min_dist_in_prev, prev_row[j]);
    }

    if (pq.size() == k) {
        // 当前分支能达到的最高理论分值：子树最大词频 - 最小编辑代价惩罚
        double max_potential_score = max_weight - (min_dist_in_prev * config.heuristic_lambda);
        double k_worst_score = pq.top().log_p_i - (pq.top().edit_distance * config.heuristic_lambda);
        if (max_potential_score < k_worst_score) return;
    }

    // ==========================================
    // 2. 局部束搜索 (Local Beam Search)
    // ==========================================
    std::vector<LayerCandidate> layer_candidates;

    for (char c : valid_charset) {
        int next_state = dat.get_next_state(current_state, c);
        if (next_state == -1) continue;

        LayerCandidate lc;
        lc.c = c;
        lc.state = next_state;
        double edit_cost = get_dynamic_edit_cost(current_depth + 1, false);
        
        lc.row[0] = prev_row[0] + edit_cost;
        double min_dist_in_lc = lc.row[0];

        for (int j = 1; j <= query_len; ++j) {
            double ins = lc.row[j - 1] + 1.0;
            double del = prev_row[j] + edit_cost;
            double sub = prev_row[j - 1] + get_sub_cost(c, query[j - 1]);
            lc.row[j] = std::min({ins, del, sub});

            // 物理连击通道 (Repeat Channel): 适配噪声发生器的 repeat 模式
            if (c == query[j - 1] && j >= 2 && query[j - 1] == query[j - 2]) {
                lc.row[j] = std::min(lc.row[j], prev_row[j - 1] + 0.1); 
            }
            min_dist_in_lc = std::min(min_dist_in_lc, lc.row[j]);
        }

        // 仅保留低于阈值的物理状态
        if (min_dist_in_lc <= dynamic_threshold) {
            lc.heuristic_score = dat.get_max_weight(next_state) - (min_dist_in_lc * config.heuristic_lambda);
            layer_candidates.push_back(lc);
        }
    }

    // 对同一深度的子节点按“潜力”排序
    std::sort(layer_candidates.begin(), layer_candidates.end(), std::greater<LayerCandidate>());
    
    // 受限于 local_beam_width 的宽度截断
    size_t limit = std::min(layer_candidates.size(), (size_t)config.local_beam_width);

    for (size_t i = 0; i < limit; ++i) {
        const auto& lc = layer_candidates[i];
        std::string next_prefix = current_prefix + lc.c;
        
        // 词尾判定：使用 Sign-Bit 逻辑检测
        if (dat.is_word_end(lc.state)) {
            double final_dist = lc.row[query_len];
            // 红线三：动态相对编辑界限限制 (例如长度为2的输入至多仅允许1次有效修改)
            double max_allowed_dist = std::ceil(config.max_edit_dist_ratio * query_len);
            
            if (final_dist <= dynamic_threshold && final_dist <= max_allowed_dist) {
                // 触达门控斩杀防线：全面核验相对长度与字符集覆盖度
                if (passes_feasibility_gating(next_prefix, query, config)) {
                    double prior_log_p = dat.get_max_weight(lc.state); 
                    // 执行融合骨架特征与平滑先验指标的信道综合打分
                    double final_penalty = calculate_composite_penalty(next_prefix, query, final_dist, prior_log_p, config);
                    
                    pq.push({next_prefix, final_dist, prior_log_p, lc.state, final_penalty});
                    
                    if (pq.size() > k) {
                        pq.pop();
                        // 利用当前保留空间中最劣解的编辑距离提供安全缓冲，加速全局剪枝
                        dynamic_threshold = std::min(dynamic_threshold, pq.top().edit_distance + 1.0);
                    }
                }
            }
        }
        
        // 向下递归
        dfs(lc.state, next_prefix, query, lc.row, prev_row, lc.c, query_len, dynamic_threshold, pq, k, config);
    }
}