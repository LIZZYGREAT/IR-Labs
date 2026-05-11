#include "../include/viterbi_decoder.h"
#include <algorithm>
#include <limits>
#include <unordered_map>

struct BeamHypothesis {
    std::string text;
    double score;
    int last_state_id;
    
    bool operator<(const BeamHypothesis& other) const {
        return score > other.score; 
    }
};

ViterbiDecoder::ViterbiDecoder(
    const BigramLM& language_model, 
    double lambda_val, 
    size_t beam_width_val, 
    double base_bonus,
    double rare_thresh,
    double len_weight,
    double f_weight
) : lm(language_model), 
    lambda(lambda_val), 
    beam_width(beam_width_val), 
    base_zero_bonus(base_bonus),
    rare_threshold(rare_thresh),
    length_weight(len_weight),
    freq_weight(f_weight) {}

std::vector<DecodedSentence> ViterbiDecoder::decode(const std::vector<std::vector<Candidate>>& token_candidates) const {
    if (token_candidates.empty()) return {};

    int num_tokens = token_candidates.size();
    std::vector<BeamHypothesis> current_beam;

    // 1. 初始化 t=0 
    const auto& first_cands = token_candidates[0];
    for (const auto& cand : first_cands) {
        double bonus = calculate_dynamic_bonus(cand);
        double emission_log_p = -lambda * cand.edit_distance + bonus;
        
        double score = cand.log_p_i + emission_log_p;
        current_beam.push_back({cand.word, score, cand.state_id});
    }

    std::sort(current_beam.begin(), current_beam.end());
    if (current_beam.size() > beam_width) current_beam.resize(beam_width);

    // 2. 推进时刻 t=1 到 t=N-1
    for (int t = 1; t < num_tokens; ++t) {
        const auto& curr_cands = token_candidates[t];
        
        // 修复：同构路径合并 (Path Merging) 防止同质化劣解占据 Beam 空间
        std::unordered_map<int, BeamHypothesis> merged_beam;

        for (const auto& hyp : current_beam) {
            for (const auto& cand : curr_cands) {
                
                double bonus = calculate_dynamic_bonus(cand);
                double emission_log_p = -lambda * cand.edit_distance + bonus;

                double transition_log_p = lm.get_transition_log_p(
                    hyp.last_state_id, 
                    cand.state_id, 
                    cand.log_p_i
                );

                double new_score = hyp.score + transition_log_p + emission_log_p;
                std::string new_text = hyp.text + " " + cand.word;

                // 检查并仅保留到达该候选状态的最优路径
                auto it = merged_beam.find(cand.state_id);
                if (it == merged_beam.end() || new_score > it->second.score) {
                    merged_beam[cand.state_id] = {new_text, new_score, cand.state_id};
                }
            }
        }

        std::vector<BeamHypothesis> next_beam;
        next_beam.reserve(merged_beam.size());
        for (const auto& pair : merged_beam) {
            next_beam.push_back(pair.second);
        }

        std::sort(next_beam.begin(), next_beam.end());
        if (next_beam.size() > beam_width) next_beam.resize(beam_width);
        
        current_beam = std::move(next_beam);
    }

    // 3. 结果封装
    std::vector<DecodedSentence> results;
    for (const auto& hyp : current_beam) {
        results.push_back({hyp.text, hyp.score});
    }

    return results;
}