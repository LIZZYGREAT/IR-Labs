import random
import os
from tqdm import tqdm

class TypoGenerator:
    """
    基于 QWERTY 键盘拓扑结构的物理噪声发生器
    """
    def __init__(self):
        # 严格定义键盘相邻拓扑关系（基于曼哈顿距离 <= 1.5 的按键）
        self.keyboard_adjacency = {
            'q': ['w', 'a', 's'], 'w': ['q', 'e', 'a', 's', 'd'], 'e': ['w', 'r', 's', 'd', 'f'],
            'r': ['e', 't', 'd', 'f', 'g'], 't': ['r', 'y', 'f', 'g', 'h'], 'y': ['t', 'u', 'g', 'h', 'j'],
            'u': ['y', 'i', 'h', 'j', 'k'], 'i': ['u', 'o', 'j', 'k', 'l'], 'o': ['i', 'p', 'k', 'l'],
            'p': ['o', 'l'], 'a': ['q', 'w', 's', 'z', 'x'], 's': ['q', 'w', 'e', 'a', 'd', 'z', 'x', 'c'],
            'd': ['w', 'e', 'r', 's', 'f', 'x', 'c', 'v'], 'f': ['e', 'r', 't', 'd', 'g', 'c', 'v', 'b'],
            'g': ['r', 't', 'y', 'f', 'h', 'v', 'b', 'n'], 'h': ['t', 'y', 'u', 'g', 'j', 'b', 'n', 'm'],
            'j': ['y', 'u', 'i', 'h', 'k', 'n', 'm'], 'k': ['u', 'i', 'o', 'j', 'l', 'm'],
            'l': ['i', 'o', 'p', 'k'], 'z': ['a', 's', 'x'], 'x': ['a', 's', 'd', 'z', 'c'],
            'c': ['s', 'd', 'f', 'x', 'v'], 'v': ['d', 'f', 'g', 'c', 'b'],
            'b': ['f', 'g', 'h', 'v', 'n'], 'n': ['g', 'h', 'j', 'b', 'm'], 'm': ['h', 'j', 'k', 'n']
        }

    def inject_noise(self, query: str, error_rate: float = 0.15) -> str:
        """
        向纯净 Query 中注入符合物理规律的噪声。
        :param query: 原始正确字符串 (意图 I)
        :param error_rate: 每个字符发生错误的绝对概率
        :return: 注入噪声后的字符串 (观测 O)
        """
        if len(query) <= 1:
            return query

        noisy_query = list(query)
        i = 0
        while i < len(noisy_query):
            # 仅对字母进行加噪
            if not noisy_query[i].isalpha():
                i += 1
                continue

            if random.random() < error_rate:
                # 确定具体的错误类型及其发生概率分布
                error_type = random.choices(
                    ['substitute', 'transpose', 'insert', 'delete'], 
                    weights=[0.60, 0.20, 0.10, 0.10] 
                )[0]

                if error_type == 'substitute':
                    # 邻近键替换
                    current_char = noisy_query[i].lower()
                    if current_char in self.keyboard_adjacency:
                        noisy_query[i] = random.choice(self.keyboard_adjacency[current_char])
                
                elif error_type == 'transpose' and i < len(noisy_query) - 1:
                    # 相邻字符交换
                    noisy_query[i], noisy_query[i+1] = noisy_query[i+1], noisy_query[i]
                    i += 1 # 跳过下一个字符，防止二次突变
                
                elif error_type == 'insert':
                    # 在当前字符旁插入一个邻近键字符
                    current_char = noisy_query[i].lower()
                    if current_char in self.keyboard_adjacency:
                        insert_char = random.choice(self.keyboard_adjacency[current_char])
                        noisy_query.insert(i, insert_char)
                        i += 1 # 调整索引以匹配新长度
                
                elif error_type == 'delete':
                    # 删除当前字符
                    noisy_query.pop(i)
                    i -= 1 # 调整索引因为列表长度缩短
            i += 1
            
        return "".join(noisy_query)

def generate_parallel_corpus(input_filepath: str, output_filepath: str, error_rate: float = 0.15):
    """
    读取纯净 Query 文件，生成对应的噪声 Query，并保存为 TSV 格式的平行语料。
    """
    if not os.path.exists(input_filepath):
        print(f"错误：找不到输入文件 {input_filepath}")
        return

    generator = TypoGenerator()
    
    # 首先统计文件行数以便渲染进度条
    print("正在扫描输入文件以确定总行数...")
    with open(input_filepath, 'r', encoding='utf-8') as f:
        total_lines = sum(1 for _ in f)

    print(f"开始生成平行语料，总条目数：{total_lines}")
    
    with open(input_filepath, 'r', encoding='utf-8') as f_in, \
         open(output_filepath, 'w', encoding='utf-8') as f_out:
        
        # 写入 TSV 表头
        f_out.write("noisy_query\tclean_query\n")
        
        for line in tqdm(f_in, total=total_lines, desc="注入物理噪声"):
            clean_query = line.strip()
            if not clean_query:
                continue
                
            noisy_query = generator.inject_noise(clean_query, error_rate=error_rate)
            
            # 即使没有发生变异（噪声命中率为概率事件），也记录下来作为负样本对照
            f_out.write(f"{noisy_query}\t{clean_query}\n")

    print(f"平行语料生成完成！已保存至：{output_filepath}")

if __name__ == "__main__":
    INPUT_FILE = "./data/processed/msmarco_queries_clean.txt"
    OUTPUT_FILE = "msmarco_parallel_corpus.tsv"
    
    generate_parallel_corpus(INPUT_FILE, OUTPUT_FILE)