#ifndef SEARCH_CONFIG_H
#define SEARCH_CONFIG_H

#include <string>

struct SearchParams {
    // 基础编辑距离阈值
    double base_threshold = 2.5;
    // 长度补偿系数 (长词自动增加容错)
    double length_compensation = 0.2;
    // 置信度计算中的物理距离权重 (lambda)
    double dist_weight = 2.0;
    // 置信度熔断阈值 (低于此分启动 Slow-Path)
    double confidence_fuse = -15.0;
    // 相对编辑距离硬性比例 (防止短词过度误纠)
    double max_error_ratio = 0.45;
    // Slow-Path 的搜索深度限制
    double fallback_limit = 2.0;
};

#endif