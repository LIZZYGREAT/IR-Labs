import re
from datasets import load_dataset
from tqdm import tqdm

def extract_queries_from_msmarco(output_filepath: str, max_samples: int = None):
    """
    从本地 MS MARCO Parquet 文件中提取纯文本 Query，并进行绝对纯净的纯字母清洗。
    """
    print("正在直接加载本地 MS MARCO Parquet 数据切片...")
    
    data_files = "./data/raw/ms_marco/v2.1/train-*.parquet"
    
    try:
        dataset = load_dataset("parquet", data_files=data_files, split="train")
    except Exception as e:
        print(f"数据加载失败，请检查相对路径是否匹配: {e}")
        return
    
    # 核心防错升级：彻底剔除数字与所有标点符号，仅保留英文字母与空格
    # 这能强制拆分类似 "what?" 为 "what"，将 "1,2" 等无意义组合直接消解为空白
    clean_pattern = re.compile(r'[^a-zA-Z\s]')
    
    total_count = len(dataset) if max_samples is None else max_samples
    extracted_count = 0
    
    with open(output_filepath, 'w', encoding='utf-8') as f:
        for item in tqdm(dataset, total=total_count, desc="提取并清洗 Query"):
            if max_samples is not None and extracted_count >= max_samples:
                break
                
            raw_query = item.get('query', '').strip()
            if not raw_query:
                continue
                
            # 基础清洗：转小写，将所有非英文字母强行替换为空格以切断粘连
            cleaned_query = clean_pattern.sub(' ', raw_query.lower())
            # 压缩连续空格
            cleaned_query = re.sub(r'\s+', ' ', cleaned_query).strip()
            
            if cleaned_query:
                f.write(cleaned_query + '\n')
                extracted_count += 1
                
    print(f"提取完成！共提取 {extracted_count} 条纯净纯字母 Query，已保存至 {output_filepath}")

if __name__ == "__main__":
    extract_queries_from_msmarco("./data/processed/msmarco_queries_clean.txt", max_samples=1000000)