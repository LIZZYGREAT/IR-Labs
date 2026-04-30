#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#endif
// ==========================================
// 1. Memory Pool LCRS Trie (Left-Child Right-Sibling)
// Extremely cache-friendly, no std::map, no heap fragmentation
// ==========================================
struct FlatTrieNode {
    int first_child; // Index in pool
    int next_sibling; // Index in pool
    char c;
    bool is_end;
    double log_p_i;

    FlatTrieNode(char character = '\0') 
        : first_child(-1), next_sibling(-1), c(character), is_end(false), log_p_i(0.0) {}
};

class ArenaTrie {
public:
    std::vector<FlatTrieNode> pool;
    int root;

    ArenaTrie() {
        // Reserve memory to avoid early reallocations
        pool.reserve(5000000); 
        pool.push_back(FlatTrieNode());
        root = 0;
    }

    void insert(const std::string& word, double log_p_i) {
        int current = root;
        for (char c : word) {
            int child = pool[current].first_child;
            int prev = -1;
            bool found = false;

            // Search in siblings
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
                // Allocate new node in contiguous memory
                int new_node_idx = pool.size();
                pool.push_back(FlatTrieNode(c));
                
                if (prev == -1) {
                    pool[current].first_child = new_node_idx;
                } else {
                    pool[prev].next_sibling = new_node_idx;
                }
                current = new_node_idx;
            }
        }
        pool[current].is_end = true;
        pool[current].log_p_i = log_p_i;
    }
};

// ==========================================
// 2. Double-Array Trie Builder with Free-List
// ==========================================
class DATBuilder {
private:
    std::vector<int> base;
    std::vector<int> check;
    std::vector<double> weights;
    
    // Free-list arrays for O(1) empty slot skipping
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
        weights.resize(new_size, 0.0);
        next_empty.resize(new_size, 0);
        prev_empty.resize(new_size, 0);

        for (size_t i = old_size; i < new_size; ++i) {
            next_empty[i] = i + 1;
            prev_empty[i] = i - 1;
        }
        next_empty[new_size - 1] = -1;
        
        int old_tail = old_size - 1;
        while (old_tail >= 0 && check[old_tail] != 0) {
            old_tail--;
        }

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
        weights.resize(initial_capacity, 0.0);
        next_empty.resize(initial_capacity, 0);
        prev_empty.resize(initial_capacity, 0);

        for (size_t i = 0; i < initial_capacity; ++i) {
            next_empty[i] = i + 1;
            prev_empty[i] = i - 1;
        }
        next_empty[initial_capacity - 1] = -1;
        prev_empty[0] = -1;
        head_empty = 0;

        // Occupy state 0 (root) and 1 (usually reserved)
        occupy_state(0);
        occupy_state(1);
        check[1] = 1;
        base[1] = 1;
        max_state_id = 1;
    }

    void build_from_arena(const ArenaTrie& trie) {
        std::cout << "[INFO] Commencing DAT construction using strict Free-List Strategy..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::queue<std::pair<int, int>> q; // {trie_node_idx, dat_state}
        q.push({trie.root, 1});

        int processed_nodes = 0;

        while (!q.empty()) {
            auto current = q.front();
            q.pop();
            int node_idx = current.first;
            int current_state = current.second;

            processed_nodes++;
            if (processed_nodes % 500000 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                std::cout << "  -> Processed " << processed_nodes << " nodes. Time elapsed: " 
                          << std::fixed << std::setprecision(2) << ms / 1000.0 << "s" << std::endl;
            }

            // Extract edges using sibling traversal
            std::vector<std::pair<char, int>> edges;
            int child_idx = trie.pool[node_idx].first_child;
            while (child_idx != -1) {
                edges.push_back({trie.pool[child_idx].c, child_idx});
                child_idx = trie.pool[child_idx].next_sibling;
            }

            if (edges.empty()) continue;

            // Sort edges by character code (crucial for DAT structure)
            std::sort(edges.begin(), edges.end(), [](const std::pair<char, int>& a, const std::pair<char, int>& b) {
                return static_cast<unsigned char>(a.first) < static_cast<unsigned char>(b.first);
            });

            int min_char_code = get_char_code(edges[0].first);
            int base_val = -1;
            bool valid = false; // 【逻辑修复】：将 valid 提升到外层，强制约束状态机
            int search_ptr = head_empty;

            // Free-list Search: O(1) collision skip with guaranteed valid check
            while (!valid) {
                // 【逻辑修复】：如果空闲链表被耗尽，说明当前内存实在太拥挤了，强制扩容
                if (search_ptr == -1) {
                    size_t old_size = check.size();
                    expand_memory(old_size + old_size / 2); // 每次扩容 50%
                    search_ptr = old_size; // 指针重置到新开辟的纯净空闲区的头部
                }

                base_val = search_ptr - min_char_code;
                
                if (base_val > 0) {
                    valid = true;
                    // Check all siblings
                    for (const auto& edge : edges) {
                        int target_state = base_val + get_char_code(edge.first);
                        
                        if (target_state >= static_cast<int>(check.size())) {
                            expand_memory((std::max)(static_cast<size_t>(target_state) + 1, check.size() * 2));
                        }

                        if (check[target_state] != 0) {
                            valid = false;
                            break; // 只要有一个子节点碰撞，当前 base_val 作废
                        }
                    }
                }
                
                // 如果当前物理槽位验证失败，指针向后移动寻找下一个空洞
                if (!valid) {
                    search_ptr = next_empty[search_ptr];
                }
            }

            // 【安全断言】：运行到这里时，valid 必定为 true，绝对不会存在原址脏写的可能
            base[current_state] = base_val;

            for (const auto& edge : edges) {
                char c = edge.first;
                int child_node_idx = edge.second;
                int target_state = base_val + get_char_code(c);
                
                check[target_state] = current_state;
                occupy_state(target_state);

                if (target_state > max_state_id) {
                    max_state_id = target_state;
                }

                if (trie.pool[child_node_idx].is_end) {
                    weights[target_state] = trie.pool[child_node_idx].log_p_i;
                }

                q.push({child_node_idx, target_state});
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "[SUCCESS] DAT Built. Max State ID: " << max_state_id 
                  << " | Core Build Time: " << duration / 1000.0 << "s" << std::endl;
    }

    // High-performance Memory-Mapped I/O Flush (Windows API)
    void export_mmap(const std::string& output_dir) {
        std::cout << "[INFO] Commencing Zero-Copy MMAP disk flush (Windows API)..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        size_t actual_size = max_state_id + 1;
        
        auto write_bin = [&](const std::string& filename, const void* data, size_t bytes) {
            std::string full_path = output_dir + "/" + filename;
            
            // 1. 创建或打开文件
            HANDLE hFile = CreateFileA(full_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERROR] Failed to open/create " << full_path << std::endl;
                return;
            }

            // 2. 创建文件映射对象 (预分配磁盘空间)
            // 注意：Windows API 需要高位和低位两个 DWORD 来表示 64 位大小，这里假设 bytes 不会超过 4GB (32位最大值)
            HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(bytes), NULL);
            if (hMap == NULL) {
                std::cerr << "[ERROR] CreateFileMapping failed for " << full_path << std::endl;
                CloseHandle(hFile);
                return;
            }

            // 3. 将文件视图映射到进程的虚拟内存中
            void* map_ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
            if (map_ptr == NULL) {
                std::cerr << "[ERROR] MapViewOfFile failed for " << full_path << std::endl;
                CloseHandle(hMap);
                CloseHandle(hFile);
                return;
            }

            // 4. 零拷贝内存冲刷
            std::memcpy(map_ptr, data, bytes);
            
            // 5. 强制操作系统将脏页 (Dirty Pages) 刷新到物理磁盘
            FlushViewOfFile(map_ptr, bytes);
            
            // 6. 清理句柄
            UnmapViewOfFile(map_ptr);
            CloseHandle(hMap);
            CloseHandle(hFile);
        };

        write_bin("base.bin", base.data(), actual_size * sizeof(int));
        write_bin("check.bin", check.data(), actual_size * sizeof(int));
        write_bin("weights.bin", weights.data(), actual_size * sizeof(double));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        std::cout << "[SUCCESS] Windows MMAP Flush Completed in " << duration / 1000.0 << "s. Path: " << output_dir << std::endl;
    }
};
// ==========================================
// 3. Offline Pipeline Main 
// ==========================================
int main() {
    auto global_start = std::chrono::high_resolution_clock::now();

    std::string input_file = "../data/processed/unigram_priors.tsv";
    std::string output_dir = "../data/index";

    ArenaTrie trie;
    std::ifstream infile(input_file);

    if (!infile.is_open()) {
        std::cerr << "[FATAL] Invalid input path: " << input_file << std::endl;
        return 1;
    }

    std::cout << "[INFO] Phase 1: Parsing TSV & Constructing Arena Trie..." << std::endl;
    auto tsv_start = std::chrono::high_resolution_clock::now();
    
    std::string line;
    std::getline(infile, line); // Skip header

    int line_count = 0;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        size_t first_tab = line.find('\t');
        size_t second_tab = line.find('\t', first_tab + 1);

        if (first_tab != std::string::npos && second_tab != std::string::npos) {
            std::string ngram = line.substr(0, first_tab);
            double log_p_i = std::stod(line.substr(second_tab + 1));
            trie.insert(ngram, log_p_i);
            line_count++;
        }
    }
    infile.close();
    
    auto tsv_end = std::chrono::high_resolution_clock::now();
    std::cout << "[SUCCESS] Loaded " << line_count << " entries. Phase 1 took: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(tsv_end - tsv_start).count() / 1000.0 << "s" << std::endl;

    std::cout << "\n[INFO] Phase 2: Double-Array Trie State Mapping..." << std::endl;
    DATBuilder dat_builder;
    dat_builder.build_from_arena(trie);

    std::cout << "\n[INFO] Phase 3: Posix Memory-Mapped I/O Persistence..." << std::endl;
    dat_builder.export_mmap(output_dir);

    auto global_end = std::chrono::high_resolution_clock::now();
    auto global_duration = std::chrono::duration_cast<std::chrono::milliseconds>(global_end - global_start).count();
    
    std::cout << "\n==========================================" << std::endl;
    std::cout << "[FINAL] Total Pipeline Execution Time: " << global_duration / 1000.0 << " seconds" << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}