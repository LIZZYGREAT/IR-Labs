#include "../include/trie_dp_search.h"
#include <cmath>

TrieDPSearcher::TrieDPSearcher(const DoubleArrayTrie& dat_instance) 
    : dat(dat_instance) {
    init_cost_table(); 
}

void TrieDPSearcher::init_cost_table() {
    for (int i = 0; i < 128; ++i) {
        for (int j = 0; j < 128; ++j) {
            sub_cost_table[i][j] = 2.0; 
        }
    }

    struct Coord { int r; int c; };
    Coord keyboard[128];
    for (int i = 0; i < 128; ++i) keyboard[i] = {-1, -1};

    keyboard[static_cast<int>('q')] = {0, 0}; keyboard[static_cast<int>('w')] = {0, 1}; keyboard[static_cast<int>('e')] = {0, 2}; keyboard[static_cast<int>('r')] = {0, 3}; keyboard[static_cast<int>('t')] = {0, 4};
    keyboard[static_cast<int>('y')] = {0, 5}; keyboard[static_cast<int>('u')] = {0, 6}; keyboard[static_cast<int>('i')] = {0, 7}; keyboard[static_cast<int>('o')] = {0, 8}; keyboard[static_cast<int>('p')] = {0, 9};
    keyboard[static_cast<int>('a')] = {1, 0}; keyboard[static_cast<int>('s')] = {1, 1}; keyboard[static_cast<int>('d')] = {1, 2}; keyboard[static_cast<int>('f')] = {1, 3}; keyboard[static_cast<int>('g')] = {1, 4};
    keyboard[static_cast<int>('h')] = {1, 5}; keyboard[static_cast<int>('j')] = {1, 6}; keyboard[static_cast<int>('k')] = {1, 7}; keyboard[static_cast<int>('l')] = {1, 8};
    keyboard[static_cast<int>('z')] = {2, 1}; keyboard[static_cast<int>('x')] = {2, 2}; keyboard[static_cast<int>('c')] = {2, 3}; keyboard[static_cast<int>('v')] = {2, 4}; keyboard[static_cast<int>('b')] = {2, 5};
    keyboard[static_cast<int>('n')] = {2, 6}; keyboard[static_cast<int>('m')] = {2, 7};

    for (char c1 = 'a'; c1 <= 'z'; ++c1) {
        for (char c2 = 'a'; c2 <= 'z'; ++c2) {
            int ic1 = static_cast<int>(c1);
            int ic2 = static_cast<int>(c2);
            
            if (c1 == c2) {
                sub_cost_table[ic1][ic2] = 0.0; 
            } else {
                Coord p1 = keyboard[ic1];
                Coord p2 = keyboard[ic2];
                if (p1.r == -1 || p2.r == -1) continue;
                int dist = std::abs(p1.r - p2.r) + std::abs(p1.c - p2.c);
                double cost = 0.6 + (0.2 * dist);
                sub_cost_table[ic1][ic2] = std::min(cost, 2.0);
            }
        }
    }
}

// 接收外部传入的 params
std::vector<Candidate> TrieDPSearcher::search(const std::string& query, const SearchParams& params, size_t k) const {
    int query_len = query.length();
    if (query_len == 0 || query_len > MAX_QUERY_LEN - 1) return {};

    double adaptive_threshold = params.base_threshold;
    if (query_len > 5) {
        adaptive_threshold += (query_len - 5) * params.length_compensation;
    }

    double root_row[MAX_QUERY_LEN + 1]; 
    root_row[0] = 0.0;
    for (int j = 1; j <= query_len; ++j) {
        root_row[j] = root_row[j - 1] + get_ins_cost(query[j - 1]);
    }

    std::priority_queue<Candidate> pq;
    double current_dynamic_limit = adaptive_threshold;

    char first_char = query[0];
    int first_state = dat.get_next_state(dat.get_root_state(), first_char);
    
    if (first_state != -1) {
        double first_row[MAX_QUERY_LEN + 1];
        first_row[0] = root_row[0] + get_del_cost(first_char);
        for (int j = 1; j <= query_len; ++j) {
            double ins = first_row[j - 1] + get_ins_cost(query[j - 1]);
            double del = root_row[j] + get_del_cost(first_char);
            double sub = root_row[j - 1] + get_sub_cost(first_char, query[j - 1]);
            first_row[j] = std::min({ins, del, sub});
        }
        std::string prefix(1, first_char);
        // 透传 params
        dfs(first_state, prefix, query, first_row, query_len, current_dynamic_limit, pq, k, params);
    }

    bool need_fallback = false;
    if (pq.empty()) {
        need_fallback = true;
    } else {
        const Candidate& best = pq.top();
        double confidence = best.log_p_i - (best.edit_distance * params.dist_weight);
        
        if (confidence < params.confidence_fuse || best.edit_distance > (adaptive_threshold * 0.75)) {
            need_fallback = true;
        }
    }

    if (need_fallback) {
        double fallback_limit = std::min(current_dynamic_limit, params.fallback_limit); 

        for (char c : valid_charset) {
            if (c == first_char) continue; 
            int next_state = dat.get_next_state(dat.get_root_state(), c);
            if (next_state == -1) continue;

            double curr_row[MAX_QUERY_LEN + 1];
            curr_row[0] = root_row[0] + get_del_cost(c);
            for (int j = 1; j <= query_len; ++j) {
                double ins = curr_row[j - 1] + get_ins_cost(query[j - 1]);
                double del = root_row[j] + get_del_cost(c);
                double sub = root_row[j - 1] + get_sub_cost(c, query[j - 1]);
                curr_row[j] = std::min({ins, del, sub});
            }
            std::string prefix(1, c);
            dfs(next_state, prefix, query, curr_row, query_len, fallback_limit, pq, k, params);
        }
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
    int query_len,
    double& dynamic_threshold, 
    std::priority_queue<Candidate>& pq, 
    size_t k,
    const SearchParams& config
) const {

    if (pq.size() == k) {
        dynamic_threshold = std::min(dynamic_threshold, pq.top().edit_distance);
    }

    for (char c : valid_charset) {
        int next_state = dat.get_next_state(current_state, c);
        if (next_state == -1) continue;

        double curr_row[MAX_QUERY_LEN + 1];
        curr_row[0] = prev_row[0] + get_del_cost(c);
        double min_dist = curr_row[0];

        for (int j = 1; j <= query_len; ++j) {
            double ins = curr_row[j - 1] + get_ins_cost(query[j - 1]);
            double del = prev_row[j] + get_del_cost(c);
            double sub = prev_row[j - 1] + get_sub_cost(c, query[j - 1]);
            curr_row[j] = std::min({ins, del, sub});
            min_dist = std::min(min_dist, curr_row[j]);
        }

        if (min_dist <= dynamic_threshold && (min_dist / static_cast<double>(query_len)) <= config.max_error_ratio) {
            std::string next_prefix = current_prefix + c;
            
            if (dat.is_word_end(next_state)) {
                double dist = curr_row[query_len];
                if (dist <= dynamic_threshold) {
                    pq.push({next_prefix, dist, dat.get_weight(next_state), next_state});
                    if (pq.size() > k) {
                        pq.pop();
                        dynamic_threshold = std::min(dynamic_threshold, pq.top().edit_distance);
                    }
                }
            }
            dfs(next_state, next_prefix, query, curr_row, query_len, dynamic_threshold, pq, k, config);
        }
    }
}