import random
import os
import math
import re
from tqdm import tqdm
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Dict, Set, Tuple, List

@dataclass
class NoiseConfig:
    """
    集中式噪声参数配置类 (Benchmark 生产级)
    作用域约束：所有概率计算严格基于单个 Token 长度
    """
    # 基础参数
    base_error_rate: float = 0.20  # P_base (极短词错误率上限)
    alpha: float = 0.231           # 长度衰减系数 (由 P_base=0.20, L=7, P=0.05 严谨推导)
    max_retries: int = 3
    max_keyboard_dist: float = 1.5
    
    # 认知豁免参数 (Scheme A: 静态常数全局豁免)
    cognitive_bypass_prob: float = 0.30 
    
    # 分段错误转移矩阵 (Piecewise Error Transition Matrix)
    # 向量索引严格对应: [substitute, transpose, insert, delete, repeat]
    short_word_weights: List[float] = field(default_factory=lambda: [0.70, 0.25, 0.05, 0.00, 0.00])
    mid_word_weights: List[float] = field(default_factory=lambda: [0.50, 0.15, 0.10, 0.15, 0.10])
    long_word_weights: List[float] = field(default_factory=lambda: [0.35, 0.10, 0.15, 0.25, 0.15])


def extract_statistics(unigram_filepath: str) -> Tuple[Dict[str, int], Set[str]]:
    """
    从词典提取字符频率与合法 Bigram 先验分布。
    数据清洗准则：强制过滤长度 < 2 且包含非 [a-z] 字母的污染条目。
    """
    print(f"正在从 {unigram_filepath} 提取物理与认知先验特征...")
    char_freq = defaultdict(int)
    valid_bigrams = set()
    
    if not os.path.exists(unigram_filepath):
        print(f"[警告] 找不到词典文件 {unigram_filepath}，系统将退化为均匀分布。")
        return char_freq, valid_bigrams

    with open(unigram_filepath, 'r', encoding='utf-8') as f:
        is_first_line = True
        for line in f:
            line = line.strip()
            if not line: continue
            parts = line.split('\t')
            
            if is_first_line:
                is_first_line = False
                if "word" in parts[0].lower() or (len(parts) > 1 and "freq" in parts[1].lower()):
                    continue
            
            word = parts[0].lower()
            
            # 严格防污染：丢弃孤立单字母及包含标点符号的数据
            if len(word) < 2 or not word.isalpha():
                continue
            
            try:
                freq = int(parts[1]) if len(parts) > 1 else 1
            except ValueError:
                freq = 1
            
            for i in range(len(word)):
                char_freq[word[i]] += freq
                if i < len(word) - 1:
                    valid_bigrams.add(word[i:i+2])
                    
    print(f"[成功] 特征提取完毕！载入有效字符：{len(char_freq)}，合法拼接规则：{len(valid_bigrams)}")
    return char_freq, valid_bigrams


class RealisticNoiseGenerator:
    """
    基于分段转移矩阵与动态衰减的高拟真 Token 级噪声发生器
    """
    def __init__(self, char_freq_dict: Dict[str, int], valid_char_bigrams: Set[str], config: NoiseConfig = NoiseConfig()):
        self.char_freq = char_freq_dict
        self.valid_bigrams = valid_char_bigrams
        self.config = config
        self.keyboard = {
            'q':(0,0), 'w':(0,1), 'e':(0,2), 'r':(0,3), 't':(0,4), 'y':(0,5), 'u':(0,6), 'i':(0,7), 'o':(0,8), 'p':(0,9),
            'a':(1,0), 's':(1,1), 'd':(1,2), 'f':(1,3), 'g':(1,4), 'h':(1,5), 'j':(1,6), 'k':(1,7), 'l':(1,8),
            'z':(2,1), 'x':(2,2), 'c':(2,3), 'v':(2,4), 'b':(2,5), 'n':(2,6), 'm':(2,7)
        }
        self.adjacent_keys = self._build_adjacency(self.config.max_keyboard_dist)

    def _build_adjacency(self, max_dist: float) -> Dict[str, List[Tuple[str, float]]]:
        adj = defaultdict(list)
        for c1, p1 in self.keyboard.items():
            for c2, p2 in self.keyboard.items():
                if c1 == c2: continue
                dist = abs(p1[0] - p2[0]) + abs(p1[1] - p2[1])
                if dist <= max_dist:
                    freq = self.char_freq.get(c2, 1)
                    # 距离衰减与频率对数加权
                    weight = math.log(1 + freq) / math.exp(0.5 * dist) 
                    adj[c1].append((c2, weight))
        
        # 概率归一化
        for c, neighbors in adj.items():
            total_w = sum(w for _, w in neighbors)
            adj[c] = [(n, w / total_w) for n, w in neighbors]
        return adj

    def _get_error_type_weights(self, word_len: int) -> List[float]:
        """
        基于当前 Token 长度的查表路由
        """
        if word_len <= 3:
            return self.config.short_word_weights
        elif word_len <= 6:
            return self.config.mid_word_weights
        else:
            return self.config.long_word_weights

    def _eval_token_cognitive_filter(self, token: str) -> bool:
        """
        Token 级认知防线校验。
        评估单一字符串是否满足物理人类打字习惯，或通过 Scheme A 全局概率豁免。
        """
        if not self.valid_bigrams: return True 
        
        token_lower = token.lower()
        if len(token_lower) < 2: return True
        
        for i in range(len(token_lower) - 1):
            bigram = token_lower[i:i+2]
            if bigram[0].isalpha() and bigram[1].isalpha():
                if bigram not in self.valid_bigrams:
                    # Scheme A 决策树：未命中豁免率则拒绝
                    if random.random() > self.config.cognitive_bypass_prob:
                        return False
        return True

    def inject_noise(self, query: str) -> str:
        """
        句子级调度器入口。
        使用正则拆分保留原句结构，确保噪声注入的边界严格收敛于单一 Token。
        """
        # 拆分策略：使用捕获组保留所有的空白字符块
        tokens = re.split(r'(\s+)', query)
        noisy_tokens = []
        
        for token in tokens:
            if not token.strip():
                noisy_tokens.append(token) # 直接透传空格和制表符
            else:
                noisy_tokens.append(self._inject_token_noise(token))
                
        return "".join(noisy_tokens)

    def _inject_token_noise(self, token: str) -> str:
        """
        Token 级物理变异引擎。
        包含动态衰减、分段转移及多类型按键异常模拟。
        """
        # 前置过滤：拒绝处理过短或包含标点符号的 Token
        if len(token) <= 2 or not token.isalpha():
            return token

        token_len = len(token)
        # 指数衰减模型求解：P_error(L) = P_base * e^(-alpha * (L-1))
        effective_error_rate = self.config.base_error_rate * math.exp(-self.config.alpha * (token_len - 1))

        for attempt in range(self.config.max_retries):
            noisy_token = list(token)
            i = 0
            mutated = False
            
            while i < len(noisy_token):
                if not noisy_token[i].isalpha():
                    i += 1
                    continue

                if random.random() < effective_error_rate:
                    weights = self._get_error_type_weights(token_len)
                    error_type = random.choices(
                        ['substitute', 'transpose', 'insert', 'delete', 'repeat'], 
                        weights=weights 
                    )[0]

                    if error_type == 'substitute':
                        current_char = noisy_token[i].lower()
                        if current_char in self.adjacent_keys:
                            neighbors = self.adjacent_keys[current_char]
                            r = random.random()
                            cum_w = 0.0
                            for n, w in neighbors:
                                cum_w += w
                                if r <= cum_w:
                                    noisy_token[i] = n if noisy_token[i].islower() else n.upper()
                                    mutated = True
                                    break
                    
                    elif error_type == 'transpose' and i < len(noisy_token) - 1:
                        if noisy_token[i+1].isalpha():
                            noisy_token[i], noisy_token[i+1] = noisy_token[i+1], noisy_token[i]
                            mutated = True
                            i += 1
                    
                    elif error_type == 'insert':
                        current_char = noisy_token[i].lower()
                        if current_char in self.adjacent_keys:
                            insert_char = random.choices(
                                [n for n, w in self.adjacent_keys[current_char]],
                                weights=[w for n, w in self.adjacent_keys[current_char]]
                            )[0]
                            noisy_token.insert(i, insert_char)
                            mutated = True
                            i += 1 
                            
                    elif error_type == 'repeat':
                        # 物理防线模拟：键盘连击现象
                        noisy_token.insert(i + 1, noisy_token[i])
                        mutated = True
                        i += 1
                    
                    elif error_type == 'delete':
                        # 结构防线校验：禁止将长词删退为孤立单字母
                        if len(noisy_token) > 2:
                            noisy_token.pop(i)
                            mutated = True
                            i -= 1 
                i += 1
                
            noisy_str = "".join(noisy_token)
            
            # 校验变异产物是否符合物理与认知联合防线
            if mutated and self._eval_token_cognitive_filter(noisy_str):
                return noisy_str
                
        return token


def generate_parallel_corpus(input_filepath: str, output_filepath: str, generator: RealisticNoiseGenerator):
    """
    长句语料流水线
    """
    if not os.path.exists(input_filepath):
        print(f"[致命错误] 找不到输入语料 {input_filepath}")
        return

    print("正在扫描数据集规模...")
    with open(input_filepath, 'r', encoding='utf-8') as f:
        total_lines = sum(1 for _ in f)

    with open(input_filepath, 'r', encoding='utf-8') as f_in, \
         open(output_filepath, 'w', encoding='utf-8') as f_out:
        f_out.write("noisy_query\tclean_query\n")
        
        for line in tqdm(f_in, total=total_lines, desc="[注入高拟真物理噪声]"):
            clean_query = line.strip()
            if not clean_query: continue
            
            noisy_query = generator.inject_noise(clean_query)
            f_out.write(f"{noisy_query}\t{clean_query}\n")


def build_weighted_token_benchmark(input_parallel_corpus: str, output_token_corpus: str):
    """
    离线 Benchmark 聚合逻辑。
    对语料进行严格的二次提纯，阻断任何因切分错误产生的标点与单字母。
    """
    print("\n正在聚合并构建加权 Token 级测试集...")
    error_pairs_freq = defaultdict(int)
    
    if not os.path.exists(input_parallel_corpus):
        print(f"[致命错误] 未找到平行语料库 {input_parallel_corpus}")
        return

    with open(input_parallel_corpus, 'r', encoding='utf-8') as f:
        is_header = True
        for line in f:
            if is_header:
                is_header = False
                continue
            
            parts = line.strip().split('\t')
            if len(parts) != 2: continue
            
            noisy_tokens = parts[0].split()
            clean_tokens = parts[1].split()
            
            if len(noisy_tokens) == len(clean_tokens):
                for nt, ct in zip(noisy_tokens, clean_tokens):
                    # 数据级净化：剔除标点、单字母、非纯字母结构
                    if nt != ct and nt.isalpha() and ct.isalpha() and len(ct) >= 2:
                        error_pairs_freq[(nt, ct)] += 1
                        
    with open(output_token_corpus, 'w', encoding='utf-8') as f:
        f.write("noisy_token\tclean_token\tfrequency\n")
        # 按照频率降序序列化，符合检索系统的物理流量特征
        for (nt, ct), freq in sorted(error_pairs_freq.items(), key=lambda x: x[1], reverse=True):
            f.write(f"{nt}\t{ct}\t{freq}\n")
            
    print(f"[成功] 测试集构建结束。封存有效对照组：{len(error_pairs_freq)} 条。")


if __name__ == "__main__":
    # 工程环境路径锚定
    UNIGRAM_FILE = "./data/processed/unigram_priors.tsv"
    CLEAN_QUERIES = "./data/processed/msmarco_queries_clean.txt"
    PARALLEL_CORPUS = "./data/processed/msmarco_parallel_corpus.tsv"
    WEIGHTED_BENCHMARK = "./data/processed/weighted_benchmark_tokens.tsv"
    
    # 架构初始化
    config = NoiseConfig()
    char_freq, valid_bigrams = extract_statistics(UNIGRAM_FILE)
    generator = RealisticNoiseGenerator(char_freq, valid_bigrams, config=config)
    
    # 流水线执行
    if not os.path.exists(PARALLEL_CORPUS):
        generate_parallel_corpus(CLEAN_QUERIES, PARALLEL_CORPUS, generator)
    
    build_weighted_token_benchmark(PARALLEL_CORPUS, WEIGHTED_BENCHMARK)