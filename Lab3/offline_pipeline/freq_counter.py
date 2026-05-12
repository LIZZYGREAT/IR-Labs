import math
import os
from collections import defaultdict
from tqdm import tqdm

def load_hunspell_dictionary(dic_filepath: str) -> set:
    """
    加载权威 Hunspell 词典白名单，剥离词缀标记。
    处理非 UTF-8 编码字符（如 Latin-1 中的 é 等字节）触发的解码异常。
    """
    valid_words = set()
    if not os.path.exists(dic_filepath):
        print(f"[警告] 找不到标准词库文件 {dic_filepath}，将退化为仅依赖频次截断！")
        return valid_words
        
    print(f"正在加载静态标准词典白名单: {dic_filepath}")
    # 修改位置：在 open 函数中显式加入 errors='ignore' 参数，避免非法字节中断流水线
    with open(dic_filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # 剥离 Hunspell 斜杠后缀词缀规则 (例如 "hello/AG" -> "hello")
            word = line.split('/')[0].strip().lower()
            if word and word.isalpha():
                valid_words.add(word)
                
    print(f"成功载入 {len(valid_words)} 个权威标准基础词汇。")
    return valid_words

def calculate_dynamic_threshold(length: int, alpha: float = 10.0, beta: float = 0.3) -> int:
    """
    计算基于长度衰减的动态截断阈值。
    """
    threshold = math.floor(alpha * math.exp(-beta * length))
    return max(1, threshold)

def build_decoupled_vocabulary(input_filepath: str, unigram_out: str, bigram_out: str, dic_filepath: str):
    """
    彻底解耦 Unigram 和 Bigram，同时强制挂载标准词库白名单过滤。
    """
    # 1. 预先载入绝对正确词典白名单
    standard_vocab = load_hunspell_dictionary(dic_filepath)
    use_whitelist = len(standard_vocab) > 0

    print(f"正在扫描纯字母语料以提取 N-gram 频次: {input_filepath}")
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
                
    print(f"原始聚合不重复 Unigram 词条数: {len(unigram_freq)}")
    print(f"原始聚合不重复 Bigram 词条数: {len(bigram_freq)}")
    
    # ---------------- 1. 处理 Unigram (供 DAT 使用) ----------------
    valid_unigrams = {}
    total_unigram_freq = 0
    skipped_unigram_count = 0
    
    for word, freq in tqdm(unigram_freq.items(), desc="Unigram 白名单与动态截断"):
        # 核心拦截逻辑：只有纯字母、达到截断阈值且存在于白名单中的词方可放行
        min_required_freq = calculate_dynamic_threshold(len(word))
        
        is_standard = word in standard_vocab if use_whitelist else True
        is_high_freq_oov = freq >= 50
        
        if (is_standard or is_high_freq_oov) and freq >= min_required_freq:
            valid_unigrams[word] = freq
            total_unigram_freq += freq
        else:
            skipped_unigram_count += 1
            
    print(f"[拦截报告] 成功剔除 {skipped_unigram_count} 个非法脏词/极低频噪声。")

    # 写入 Unigram TSV
    sorted_unigrams = sorted(valid_unigrams.items(), key=lambda x: x[1], reverse=True)
    with open(unigram_out, 'w', encoding='utf-8') as f:
        f.write("ngram\tfrequency\tlog_p_i\n")
        for word, freq in tqdm(sorted_unigrams, desc="写入干净的 Unigram 先验"):
            log_p_i = math.log(freq / total_unigram_freq)
            f.write(f"{word}\t{freq}\t{log_p_i:.6f}\n")

    # ---------------- 2. 处理 Bigram (供后续 LM 使用) ----------------
    valid_bigrams = {}
    
    # 过滤 Bigram：确保转移对中的前后词都必须存在于洗净后的 valid_unigrams 空间中
    for bigram, freq in tqdm(bigram_freq.items(), desc="Bigram 拓扑安全性核验"):
        w1, w2 = bigram.split()
        if w1 in valid_unigrams and w2 in valid_unigrams:
            min_required_freq = max(1, calculate_dynamic_threshold(len(bigram), alpha=5.0))
            if freq >= min_required_freq:
                valid_bigrams[bigram] = freq

    total_bigram_freq = sum(valid_bigrams.values())
    
    # 写入 Bigram TSV
    sorted_bigrams = sorted(valid_bigrams.items(), key=lambda x: x[1], reverse=True)
    with open(bigram_out, 'w', encoding='utf-8') as f:
        f.write("bigram\tfrequency\tlog_p_transition\n")
        for bigram, freq in tqdm(sorted_bigrams, desc="写入安全 Bigram 转移矩阵"):
            log_p_transition = math.log(freq / total_bigram_freq)
            f.write(f"{bigram}\t{freq}\t{log_p_transition:.6f}\n")

    print(f"解耦与纯净度重构彻底完成！\n纯净 Unigram 路径: {unigram_out}\n安全 Bigram 路径: {bigram_out}")

if __name__ == "__main__":
    INPUT_FILE = "./data/processed/msmarco_queries_clean.txt"
    UNIGRAM_OUT = "./data/processed/unigram_priors.tsv"
    BIGRAM_OUT = "./data/processed/bigram_lm.tsv"
    DIC_FILE = "./data/raw/en_US.dic"
    
    build_decoupled_vocabulary(INPUT_FILE, UNIGRAM_OUT, BIGRAM_OUT, DIC_FILE)