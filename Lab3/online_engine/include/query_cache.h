#ifndef QUERY_CACHE_H
#define QUERY_CACHE_H

#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include "trie_dp_search.h"

// 复合 Key：包含 Query 内容和核心影响结果的参数
struct CacheKey {
    std::string query;
    double threshold;
    double ratio;

    bool operator==(const CacheKey& other) const {
        return query == other.query && threshold == other.threshold && ratio == other.ratio;
    }
};

// 为 CacheKey 提供哈希函数
struct CacheKeyHash {
    std::size_t operator()(const CacheKey& k) const {
        std::size_t h1 = std::hash<std::string>{}(k.query);
        std::size_t h2 = std::hash<double>{}(k.threshold);
        std::size_t h3 = std::hash<double>{}(k.ratio);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class QueryCache {
private:
    size_t capacity;
    std::list<std::pair<CacheKey, std::vector<Candidate>>> items;
    std::unordered_map<CacheKey, decltype(items)::iterator, CacheKeyHash> cache_map;
    mutable std::mutex mtx; // 保证多线程 Web 环境安全

public:
    QueryCache(size_t cap = 10000) : capacity(cap) {}

    std::optional<std::vector<Candidate>> get(const CacheKey& key) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it == cache_map.end()) return std::nullopt;
        
        // 移动到链表头部 (LRU 逻辑)
        items.splice(items.begin(), items, it->second);
        return it->second->second;
    }

    void put(const CacheKey& key, const std::vector<Candidate>& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            it->second->second = value;
            items.splice(items.begin(), items, it->second);
            return;
        }

        if (items.size() >= capacity) {
            auto last = items.back();
            cache_map.erase(last.first);
            items.pop_back();
        }

        items.push_front({key, value});
        cache_map[key] = items.begin();
    }
};

#endif