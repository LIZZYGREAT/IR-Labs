#ifndef DAT_BUILDER_H
#define DAT_BUILDER_H

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#endif

// ==========================================
// 1. Memory Pool LCRS Trie
// ==========================================
struct FlatTrieNode {
    int first_child; 
    int next_sibling; 
    char c;
    bool is_end;
    double log_p_i; 

    FlatTrieNode(char character = '\0') 
        : first_child(-1), next_sibling(-1), c(character), is_end(false), log_p_i(-100.0) {} 
};

class ArenaTrie {
public:
    std::vector<FlatTrieNode> pool;
    int root;

    ArenaTrie() {
        pool.reserve(10000000); // 考虑到 Query 会更大，适当调高基础容量
        pool.push_back(FlatTrieNode());
        root = 0;
    }

    void insert(const std::string& word, double log_p_i) {
        int current = root;
        for (char c : word) {
            int child = pool[current].first_child;
            int prev = -1;
            bool found = false;

            while (child != -1) {
                if (pool[child].c == c) {
                    found = true;
                    current = child;
                    break;
                }
                prev = child;
                child = pool[child].next_sibling;
            }

            if (!found) {
                int new_node_idx = pool.size();
                pool.push_back(FlatTrieNode(c));
                if (prev == -1) pool[current].first_child = new_node_idx;
                else pool[prev].next_sibling = new_node_idx;
                current = new_node_idx;
            }
        }
        pool[current].is_end = true;
        pool[current].log_p_i = log_p_i;
    }

    double compute_max_weights(int node_idx) {
        if (node_idx == -1) return -100.0;
        double current_max = pool[node_idx].is_end ? pool[node_idx].log_p_i : -100.0;
        int child = pool[node_idx].first_child;
        while (child != -1) {
            double child_max = compute_max_weights(child);
            if (child_max > current_max) {
                current_max = child_max;
            }
            child = pool[child].next_sibling;
        }
        pool[node_idx].log_p_i = current_max;
        return current_max;
    }
};

// ==========================================
// 2. Double-Array Trie Builder
// ==========================================
class DATBuilder {
private:
    std::vector<int> base;
    std::vector<int> check;
    std::vector<double> weights;
    std::vector<int> next_empty;
    std::vector<int> prev_empty;
    int head_empty;
    int max_state_id;

    inline int get_char_code(char c) const {
        return static_cast<unsigned char>(c);
    }

    void expand_memory(size_t new_size) {
        size_t old_size = check.size();
        if (new_size <= old_size) return;

        base.resize(new_size, 0);
        check.resize(new_size, 0);
        weights.resize(new_size, -100.0);
        next_empty.resize(new_size, 0);
        prev_empty.resize(new_size, 0);

        for (size_t i = old_size; i < new_size; ++i) {
            next_empty[i] = i + 1;
            prev_empty[i] = i - 1;
        }
        next_empty[new_size - 1] = -1;
        
        int old_tail = old_size - 1;
        while (old_tail >= 0 && check[old_tail] != 0) old_tail--;

        if (old_tail >= 0) {
            next_empty[old_tail] = old_size;
            prev_empty[old_size] = old_tail;
        } else {
            head_empty = old_size;
            prev_empty[old_size] = -1; 
        }
    }

    void occupy_state(int state) {
        int p = prev_empty[state];
        int n = next_empty[state];
        if (p != -1) next_empty[p] = n;
        else head_empty = n;
        if (n != -1) prev_empty[n] = p;
        next_empty[state] = -1;
        prev_empty[state] = -1;
    }

public:
    DATBuilder(size_t initial_capacity = 5000000) {
        base.resize(initial_capacity, 0);
        check.resize(initial_capacity, 0);
        weights.resize(initial_capacity, -100.0);
        next_empty.resize(initial_capacity, 0);
        prev_empty.resize(initial_capacity, 0);

        for (size_t i = 0; i < initial_capacity; ++i) {
            next_empty[i] = i + 1;
            prev_empty[i] = i - 1;
        }
        next_empty[initial_capacity - 1] = -1;
        prev_empty[0] = -1;
        head_empty = 0;

        occupy_state(0);
        occupy_state(1);
        check[1] = 1;
        base[1] = 1;
        max_state_id = 1;
    }

    void build_from_arena(const ArenaTrie& trie) {
        std::cout << "[INFO] Commencing Heuristic DAT construction..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::queue<std::pair<int, int>> q; 
        q.push({trie.root, 1});

        int processed_nodes = 0;

        while (!q.empty()) {
            auto current = q.front();
            q.pop();
            int node_idx = current.first;
            int current_state = current.second;

            processed_nodes++;
            if (processed_nodes % 500000 == 0) {
                std::cout << "  -> Processed " << processed_nodes << " nodes." << std::endl;
            }

            std::vector<std::pair<char, int>> edges;
            int child_idx = trie.pool[node_idx].first_child;
            while (child_idx != -1) {
                edges.push_back({trie.pool[child_idx].c, child_idx});
                child_idx = trie.pool[child_idx].next_sibling;
            }

            if (edges.empty()) {
                if (trie.pool[node_idx].is_end) {
                    base[current_state] = -1;
                }
                continue;
            }

            std::sort(edges.begin(), edges.end(), [](const std::pair<char, int>& a, const std::pair<char, int>& b) {
                return static_cast<unsigned char>(a.first) < static_cast<unsigned char>(b.first);
            });

            int min_char_code = get_char_code(edges[0].first);
            int base_val = -1;
            bool valid = false;
            int search_ptr = head_empty;

            while (!valid) {
                if (search_ptr == -1) {
                    size_t old_size = check.size();
                    expand_memory(old_size + old_size / 2);
                    search_ptr = old_size;
                }
                base_val = search_ptr - min_char_code;
                if (base_val > 0) {
                    valid = true;
                    for (const auto& edge : edges) {
                        int target_state = base_val + get_char_code(edge.first);
                        if (target_state >= static_cast<int>(check.size())) {
                            expand_memory(target_state + 65536);
                        }
                        if (check[target_state] != 0) {
                            valid = false;
                            break;
                        }
                    }
                }
                if (!valid) search_ptr = next_empty[search_ptr];
            }

            base[current_state] = trie.pool[node_idx].is_end ? -base_val : base_val;

            for (const auto& edge : edges) {
                int child_node_idx = edge.second;
                int target_state = base_val + get_char_code(edge.first);
                
                check[target_state] = current_state;
                occupy_state(target_state);

                weights[target_state] = trie.pool[child_node_idx].log_p_i;

                if (target_state > max_state_id) max_state_id = target_state;
                q.push({child_node_idx, target_state});
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        std::cout << "[SUCCESS] DAT Built. Duration: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000.0 << "s" << std::endl;
    }

    void export_mmap(const std::string& output_dir) {
        std::cout << "[INFO] Windows MMAP disk flush to: " << output_dir << std::endl;
        size_t actual_size = max_state_id + 1;
        
        auto write_bin = [&](const std::string& filename, const void* data, size_t bytes) {
            std::string full_path = output_dir + "/" + filename;
            HANDLE hFile = CreateFileA(full_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERROR] Failed to create file: " << full_path << std::endl;
                return;
            }
            HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(bytes), NULL);
            void* map_ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
            std::memcpy(map_ptr, data, bytes);
            FlushViewOfFile(map_ptr, bytes);
            UnmapViewOfFile(map_ptr);
            CloseHandle(hMap);
            CloseHandle(hFile);
        };

        write_bin("base.bin", base.data(), actual_size * sizeof(int));
        write_bin("check.bin", check.data(), actual_size * sizeof(int));
        write_bin("weights.bin", weights.data(), actual_size * sizeof(double));
    }
};

#endif // DAT_BUILDER_H