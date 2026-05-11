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
    std::reverse(results.begin(), results.end());
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
        // 如果潜力上限已经低于当前 Top-K 的最差分值，直接砍掉整个子树
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
            if (final_dist <= dynamic_threshold) {
                double prior_log_p = dat.get_max_weight(lc.state); // 子树最大此时即词频
                // 计算用于 Top-K 队列的排序惩罚值
                double final_penalty = final_dist - (prior_log_p * 0.1); 
                
                pq.push({next_prefix, final_dist, prior_log_p, lc.state, final_penalty});
                
                if (pq.size() > k) {
                    pq.pop();
                    // 动态更新物理阈值以加速剪枝
                    dynamic_threshold = std::min(dynamic_threshold, pq.top().edit_distance);
                }
            }
        }
        
        // 向下递归
        dfs(lc.state, next_prefix, query, lc.row, prev_row, lc.c, query_len, dynamic_threshold, pq, k, config);
    }
}