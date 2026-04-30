import math
from collections import defaultdict
from tqdm import tqdm

def calculate_dynamic_threshold(length: int, alpha: float = 10.0, beta: float = 0.3) -> int:
    """
    计算基于长度衰减的动态截断阈值。
    """
    threshold = math.floor(alpha * math.exp(-beta * length))
    return max(1, threshold)

def build_decoupled_vocabulary(input_filepath: str, unigram_out: str, bigram_out: str):
    """
    彻底解耦 Unigram 和 Bigram。
    Unigram 用于构建 DAT (Error Model)，Bigram 用于 Viterbi 解码 (Language Model)。
    """
    print(f"正在扫描原始语料以提取 N-gram 频次: {input_filepath}")
    unigram_freq = defaultdict(int)
    bigram_freq = defaultdict(int)
    
    with open(input_filepath, 'r', encoding='utf-8') as f:
        for line in tqdm(f, desc="N-gram 聚合"):
            query = line.strip()
            if not query:
                continue
                
            words = query.split()
            num_words = len(words)
            
            # 提取 Unigram
            for w in words:
                unigram_freq[w] += 1
                
            # 提取 Bigram
            for i in range(num_words - 1):
                bigram = f"{words[i]} {words[i+1]}"
                bigram_freq[bigram] += 1
                
    print(f"全局不重复 Unigram 词条数: {len(unigram_freq)}")
    print(f"全局不重复 Bigram 词条数: {len(bigram_freq)}")
    
    # ---------------- 1. 处理 Unigram (供 DAT 使用) ----------------
    valid_unigrams = {}
    total_unigram_freq = 0
    
    for word, freq in tqdm(unigram_freq.items(), desc="Unigram 动态截断"):
        min_required_freq = calculate_dynamic_threshold(len(word))
        if freq >= min_required_freq:
            valid_unigrams[word] = freq
            total_unigram_freq += freq
            
    # 写入 Unigram TSV
    sorted_unigrams = sorted(valid_unigrams.items(), key=lambda x: x[1], reverse=True)
    with open(unigram_out, 'w', encoding='utf-8') as f:
        f.write("ngram\tfrequency\tlog_p_i\n")
        for word, freq in tqdm(sorted_unigrams, desc="写入 Unigram 先验"):
            log_p_i = math.log(freq / total_unigram_freq)
            f.write(f"{word}\t{freq}\t{log_p_i:.6f}\n")

    # ---------------- 2. 处理 Bigram (供后续 LM 使用) ----------------
    valid_bigrams = {}
    total_bigram_freq = sum(bigram_freq.values())
    
    for bigram, freq in tqdm(bigram_freq.items(), desc="Bigram 动态截断"):
        # Bigram 整体更稀疏，基础门槛设低一点，防止长尾组合被过度抹杀
        min_required_freq = max(1, calculate_dynamic_threshold(len(bigram), alpha=5.0))
        if freq >= min_required_freq:
            valid_bigrams[bigram] = freq

    # 写入 Bigram TSV
    sorted_bigrams = sorted(valid_bigrams.items(), key=lambda x: x[1], reverse=True)
    with open(bigram_out, 'w', encoding='utf-8') as f:
        f.write("bigram\tfrequency\tlog_p_transition\n")
        for bigram, freq in tqdm(sorted_bigrams, desc="写入 Bigram 转移概率"):
            log_p_transition = math.log(freq / total_bigram_freq)
            f.write(f"{bigram}\t{freq}\t{log_p_transition:.6f}\n")

    print(f"解耦完成！\nUnigram 路径: {unigram_out}\nBigram 路径: {bigram_out}")

if __name__ == "__main__":
    INPUT_FILE = "./data/processed/msmarco_queries_clean.txt"
    UNIGRAM_OUT = "./data/processed/unigram_priors.tsv"
    BIGRAM_OUT = "./data/processed/bigram_lm.tsv"
    
    build_decoupled_vocabulary(INPUT_FILE, UNIGRAM_OUT, BIGRAM_OUT)