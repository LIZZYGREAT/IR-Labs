#ifndef SEARCH_CONFIG_H
#define SEARCH_CONFIG_H

struct SearchParams {
    double base_threshold = 2.5;
    double max_error_ratio = 0.45;
    double length_compensation = 0.2;
    double dist_weight = 2.0;
    double fallback_limit = 2.0;
    double confidence_fuse = -15.0;
    
    // --- 启发式与束搜索参数 ---
    double heuristic_lambda = 3.0; 
    int local_beam_width = 10;      
};

#endif