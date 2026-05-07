#include "../include/prefix_suggester.h"
#include <sstream>
#include <algorithm>

PrefixSuggester::PrefixSuggester(const DoubleArrayTrie& dat_ref, const BigramLM& lm_ref, const TrieDPSearcher& searcher_ref, int max_steps)
    : dat(dat_ref), lm(lm_ref), searcher(searcher_ref), max_dfs_steps(max_steps) {
    
    strict_config.base_threshold = 1.5;
    strict_config.fallback_limit = 1.5;
    strict_config.max_error_ratio = 0.35; 
}

void PrefixSuggester::dfs_subtree(
    int current_state, 
    const std::string& current_word, 
    const std::string& context_str, 
    int context_state,
    std::priority_queue<Suggestion>& min_heap, 
    size_t k,
    int& steps_taken
) const {
    if (steps_taken++ > max_dfs_steps) return;

    if (dat.is_word_end(current_state)) {
        double prior_log_p = dat.get_weight(current_state);
        double transition_log_p = prior_log_p;
        
        if (context_state != -1) {
            transition_log_p = lm.get_transition_log_p(context_state, current_state, prior_log_p);
        }

        double final_score = prior_log_p + transition_log_p;
        std::string full_text = context_str.empty() ? current_word : context_str + " " + current_word;
        
        min_heap.push({full_text, final_score});
        if (min_heap.size() > k) {
            min_heap.pop();
        }
    }

    for (char c = 'a'; c <= 'z'; ++c) {
        int next_state = dat.get_next_state(current_state, c);
        if (next_state != -1) {
            dfs_subtree(next_state, current_word + c, context_str, context_state, min_heap, k, steps_taken);
        }
    }
}

std::vector<Suggestion> PrefixSuggester::suggest(const std::string& raw_input, size_t k) const {
    if (raw_input.empty()) return {};

    std::vector<std::string> tokens;
    std::istringstream iss(raw_input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return {};

    bool ends_with_space = (raw_input.back() == ' ');
    std::priority_queue<Suggestion> min_heap;

    if (ends_with_space) {
        std::string last_completed_word = tokens.back();
        int context_state = dat.get_word_state(last_completed_word);

        std::string base_str = "";
        if (tokens.size() > 1) {
            base_str = raw_input.substr(0, raw_input.length() - last_completed_word.length() - 1);
        }
        std::string context_str = base_str.empty() ? last_completed_word : base_str + last_completed_word;

        if (context_state == -1) {
            auto context_corrections = searcher.search(last_completed_word, strict_config, 1);
            if (!context_corrections.empty()) {
                last_completed_word = context_corrections[0].word;
                context_state = context_corrections[0].state_id;
                context_str = base_str.empty() ? last_completed_word : base_str + last_completed_word;
            }
        }

        if (context_state != -1) {
            const auto* edges = lm.get_forward_transitions(context_state);
            if (edges) {
                size_t limit = std::min(k, edges->size());
                for (size_t i = 0; i < limit; ++i) {
                    int next_state = (*edges)[i].first;
                    double log_p = (*edges)[i].second;
                    
                    std::string next_word = lm.get_word_by_state(next_state);
                    if (!next_word.empty()) {
                        std::string full_text = context_str + " " + next_word;
                        min_heap.push({full_text, log_p});
                    }
                }
            }
        }
    } 
    else {
        std::string prefix = tokens.back();
        std::string context_str = "";
        int context_state = -1;

        if (tokens.size() > 1) {
            std::string last_completed_word = tokens[tokens.size() - 2];
            context_state = dat.get_word_state(last_completed_word);
            
            size_t prefix_pos = raw_input.find_last_of(' ');
            std::string base_str = raw_input.substr(0, prefix_pos);

            if (context_state == -1) {
                auto context_corrections = searcher.search(last_completed_word, strict_config, 1);
                if (!context_corrections.empty()) {
                    last_completed_word = context_corrections[0].word;
                    context_state = context_corrections[0].state_id;
                    
                    size_t prev_space = base_str.find_last_of(' ');
                    if (prev_space != std::string::npos) {
                        context_str = base_str.substr(0, prev_space + 1) + last_completed_word;
                    } else {
                        context_str = last_completed_word;
                    }
                } else {
                    context_str = base_str; 
                }
            } else {
                context_str = base_str;
            }
        }

        int current_state = dat.get_root_state();
        for (char c : prefix) {
            current_state = dat.get_next_state(current_state, c);
            if (current_state == -1) break; 
        }

        if (current_state != -1) {
            int steps_taken = 0;
            dfs_subtree(current_state, prefix, context_str, context_state, min_heap, k, steps_taken);
        }
        else {
            // 【核心修正】显式传入 strict_config
            auto prefix_corrections = searcher.search(prefix, strict_config, k);
            for (const auto& cand : prefix_corrections) {
                double prior_log_p = cand.log_p_i;
                double transition_log_p = prior_log_p; 
                
                if (context_state != -1) {
                    transition_log_p = lm.get_transition_log_p(context_state, cand.state_id, prior_log_p);
                }

                double final_score = prior_log_p + transition_log_p - cand.edit_distance;
                std::string full_text = context_str.empty() ? cand.word : context_str + " " + cand.word;
                
                min_heap.push({full_text, final_score});
                if (min_heap.size() > k) min_heap.pop();
            }
        }
    }

    std::vector<Suggestion> results;
    while (!min_heap.empty()) {
        results.push_back(min_heap.top());
        min_heap.pop();
    }
    std::reverse(results.begin(), results.end());
    return results;
}