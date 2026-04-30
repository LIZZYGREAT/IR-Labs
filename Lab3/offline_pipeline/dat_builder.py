import struct
import math
import os
from tqdm import tqdm

# ==========================================
# 第一阶段：构建原生的多叉字典树 (Standard Trie)
# ==========================================
class TrieNode:
    def __init__(self):
        self.children = {}
        self.is_end = False
        self.log_p_i = 0.0  # 存储该节点作为单词结尾时的先验概率对数

class StandardTrie:
    def __init__(self):
        self.root = TrieNode()

    def insert(self, word: str, log_p_i: float):
        node = self.root
        for char in word:
            if char not in node.children:
                node.children[char] = TrieNode()
            node = node.children[char]
        node.is_end = True
        node.log_p_i = log_p_i

# ==========================================
# 第二阶段：将多叉树拍扁为双数组字典树 (DAT) [启发式极限优化版]
# ==========================================
class DATBuilder:
    def __init__(self, initial_capacity=5000000):
        # 初始化三个一维数组，容量预设为 500 万
        self.base = [0] * initial_capacity
        self.check = [0] * initial_capacity
        self.weights = [0.0] * initial_capacity
        
        # DAT 的根节点通常设在索引 1
        self.check[1] = 1 
        self.base[1] = 1
        self.max_state_id = 1  
        
        # 核心优化点：启发式空闲指针，避免 O(N^2) 的盲目探测
        self.next_free_base = 1

    def _get_char_code(self, char: str) -> int:
        # 直接使用字符的 ASCII 码作为转移偏移量
        return ord(char)

    def build_from_trie(self, trie_root: TrieNode):
        print("开始将 Standard Trie 转化为 Double-Array Trie (执行空洞逼近算法)...")
        queue = [(trie_root, 1)]  
        pbar = tqdm(desc="DAT 状态转换中")
        
        while queue:
            node, current_state = queue.pop(0)
            pbar.update(1)
            
            edges = sorted(node.children.keys())
            if not edges:
                continue

            min_char_code = self._get_char_code(edges[0])
            
            # 【核心突变】：不再盲目递增 base_val，而是直接寻找下一个空槽位来安置首个字符
            empty_cursor = self.next_free_base
            
            while True:
                # 寻找真实的空洞索引
                while empty_cursor < len(self.check) and self.check[empty_cursor] != 0:
                    empty_cursor += 1
                
                # 反推可能的 base_val。必须保证 base_val > 0
                base_val = empty_cursor - min_char_code
                
                if base_val > 0:
                    valid = True
                    # 校验剩余的兄弟字符是否也能刚好落入其他空洞
                    for char in edges:
                        char_code = self._get_char_code(char)
                        target_state = base_val + char_code
                        
                        # 动态扩容
                        if target_state >= len(self.check):
                            extend_size = len(self.check) // 2
                            self.base.extend([0] * extend_size)
                            self.check.extend([0] * extend_size)
                            self.weights.extend([0.0] * extend_size)
                            
                        # 如果兄弟字符命中已被占用的槽位，当前 base_val 无效
                        if self.check[target_state] != 0:
                            valid = False
                            break
                            
                    if valid:
                        break # 找到了完美的空洞组合
                
                # 如果当前空洞不适合，继续尝试下一个空洞
                empty_cursor += 1
                
            # 找到无冲突的 base_val 后，正式写入状态
            self.base[current_state] = base_val
            
            for char in edges:
                char_code = self._get_char_code(char)
                target_state = base_val + char_code
                
                self.check[target_state] = current_state
                self.max_state_id = max(self.max_state_id, target_state)
                
                child_node = node.children[char]
                if child_node.is_end:
                    self.weights[target_state] = child_node.log_p_i
                    
                queue.append((child_node, target_state))
            
            # 维护启发式空闲指针
            while self.next_free_base < len(self.check) and self.check[self.next_free_base] != 0:
                self.next_free_base += 1
                
        pbar.close()
        print(f"转换完成！实际占用的最大数组槽位索引为: {self.max_state_id}")

    def export_binary(self, output_dir: str):
        """将数组严格截断后，导出为 C++ 可直接读取的连续内存二进制文件"""
        os.makedirs(output_dir, exist_ok=True)
        
        # 裁剪掉末尾多余的空闲空间，节省磁盘和后续 C++ 的内存
        actual_size = self.max_state_id + 1
        final_base = self.base[:actual_size]
        final_check = self.check[:actual_size]
        final_weights = self.weights[:actual_size]

        # 导出 base 数组 (32位有符号整型 'i')
        base_path = os.path.join(output_dir, "base.bin")
        with open(base_path, 'wb') as f:
            f.write(struct.pack(f'{actual_size}i', *final_base))
            
        # 导出 check 数组 (32位有符号整型 'i')
        check_path = os.path.join(output_dir, "check.bin")
        with open(check_path, 'wb') as f:
            f.write(struct.pack(f'{actual_size}i', *final_check))
            
        # 导出权重数组 (64位双精度浮点型 'd')
        weights_path = os.path.join(output_dir, "weights.bin")
        with open(weights_path, 'wb') as f:
            f.write(struct.pack(f'{actual_size}d', *final_weights))
            
        print(f"二进制索引已导出至 {output_dir}/ 目录。")
        print(f"C++ 引擎读取参数：数组长度 = {actual_size}")

def main():
    # 路径对齐你目前的物理存放结构
    INPUT_FILE = "./data/processed/ngram_vocabulary_with_priors.tsv"
    OUTPUT_DIR = "../data/index/"
    
    trie = StandardTrie()
    print("正在加载词典到内存构建 Standard Trie...")
    
    # 检测输入文件是否存在
    if not os.path.exists(INPUT_FILE):
        print(f"致命错误: 找不到输入文件 {INPUT_FILE}，请确认上一步脚本已正确执行。")
        return

    with open(INPUT_FILE, 'r', encoding='utf-8') as f:
        next(f)  # 跳过表头
        for line in tqdm(f, desc="读取词典"):
            parts = line.strip().split('\t')
            if len(parts) == 3:
                ngram, freq, log_p_i = parts
                trie.insert(ngram, float(log_p_i))
                
    dat_builder = DATBuilder()
    dat_builder.build_from_trie(trie.root)
    dat_builder.export_binary(OUTPUT_DIR)

if __name__ == "__main__":
    main()