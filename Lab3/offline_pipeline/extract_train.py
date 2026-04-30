import re
from datasets import load_dataset
from tqdm import tqdm

def extract_queries_from_msmarco(output_filepath: str, max_samples: int = None):
    """
    从本地 MS MARCO Parquet 文件中提取纯文本 Query，并进行基础清洗。
    :param output_filepath: 提取后扁平化文本的保存路径
    :param max_samples: 最大提取数量，用于快速测试（None 表示全部提取）
    """
    print("正在直接加载本地 MS MARCO Parquet 数据切片...")
    
    # 核心修改点：放弃解析原始元数据，直接使用 parquet builder 读取本地文件通配符
    data_files = "./data/raw/ms_marco/v2.1/train-*.parquet"
    
    try:
        dataset = load_dataset("parquet", data_files=data_files, split="train")
    except Exception as e:
        print(f"数据加载失败，请检查相对路径是否匹配: {e}")
        return
    
    # 预编译正则：移除非字母数字和基本标点以外的特殊字符，降低 Trie 树的无效分支
    clean_pattern = re.compile(r'[^a-zA-Z0-9\s\.\,\?]')
    
    total_count = len(dataset) if max_samples is None else max_samples
    extracted_count = 0
    
    with open(output_filepath, 'w', encoding='utf-8') as f:
        for item in tqdm(dataset, total=total_count, desc="提取 Query"):
            if max_samples is not None and extracted_count >= max_samples:
                break
                
            raw_query = item.get('query', '').strip()
            if not raw_query:
                continue
                
            # 基础清洗：转小写，去除极端特殊字符
            cleaned_query = clean_pattern.sub('', raw_query.lower())
            # 合并多个空格
            cleaned_query = re.sub(r'\s+', ' ', cleaned_query).strip()
            
            if cleaned_query:
                f.write(cleaned_query + '\n')
                extracted_count += 1
                
    print(f"提取完成！共提取 {extracted_count} 条纯净 Query，已保存至 {output_filepath}")

if __name__ == "__main__":
    extract_queries_from_msmarco("msmarco_queries_clean.txt", max_samples=1000000)