import os
from id_map import IdMap
from compression import UncompressedPostings, CompressedPostings, ECCompressedPostings
from index_io import InvertedIndexWriter, InvertedIndexIterator
from bsbi import BSBIIndex

def setup_directories():
    """初始化运行环境所需的目录结构"""
    directories = ['data/output_dir', 'data/tmp', 'data/toy_output_dir', 'data/output_dir_compressed', 'data/output_dir_ec']
    for d in directories:
        try:
            # 使用os.makedirs递归创建目录，exist_ok=True表示如果目录已存在则不报错
            os.makedirs(d, exist_ok=True)
        except Exception as e:
            print(f"创建目录 {d} 时出错: {e}")

def test_id_map():
    """测试 IdMap 的基础双向映射功能"""
    testIdMap = IdMap()
    
    assert testIdMap['a'] == 0, "Unable to add a new string to the IdMap"
    assert testIdMap['bcd'] == 1, "Unable to add a new string to the IdMap"
    
    assert testIdMap['a'] == 0, "Unable to retrieve the id of an existing string"
    
    assert testIdMap[1] == 'bcd', "Unable to retrive the string corresponding to a given id"
    
    try:
        _ = testIdMap[2]
    except IndexError:
        pass
    else:
        raise AssertionError("Doesn't throw an IndexError for out of range numeric ids")
        
    assert len(testIdMap) == 2, "Length of IdMap is incorrect"
    print("IdMap tests passed.")

def test_compression():
    """测试压缩模块的编解码一致性"""
    def _test_encode_decode(l, encoder_class):
        e = encoder_class.encode(l)
        d = encoder_class.decode(e)
        assert d == l, f"Assertion failed for {encoder_class.__name__}.\nOriginal: {l}\nDecoded: {d}"

    test_cases = [
        [1, 2, 3],                            
        [100, 105, 120, 121],                   
        [1],                                  
        [127, 128, 255, 256, 10000],           
        [10, 1000, 100000, 10000000]           
    ]

    for lst in test_cases:
        _test_encode_decode(lst, CompressedPostings)
        _test_encode_decode(lst, ECCompressedPostings)
        
    print("Compression tests passed (Variable Byte Encoding & Elias Gamma).")

def run_dev_queries(index_instance):
    """
    运行 dev_queries 下的测试集，并将检索结果与 dev_output 的标准答案比对
    """
    for i in range(1, 9):
        query_path = os.path.join('data/dev_queries', f'query.{i}')
        output_path = os.path.join('data/dev_output', f'{i}.out')
        
        if not os.path.exists(query_path) or not os.path.exists(output_path):
            print(f"Warning: dev_queries/query.{i} or dev_output/{i}.out not found. Skipping.")
            continue
            
        with open(query_path, 'r') as q:
            query = q.read().strip()
            my_results = sorted([os.path.normpath(path) for path in index_instance.retrieve(query)])
            
            with open(output_path, 'r') as o:
                reference_results = sorted([os.path.normpath(x.strip()) for x in o.readlines()])
                
                if my_results != reference_results:
                    diff_my = set(my_results) - set(reference_results)
                    diff_ref = set(reference_results) - set(my_results)
                    error_msg = f"Results DO NOT match for query: '{query}'\n"
                    error_msg += f"In my_results but missing in reference: {diff_my}\n"
                    error_msg += f"In reference but missing in my_results: {diff_ref}"
                    raise AssertionError(error_msg)
                    
            print(f"Results match for query: '{query}'")

def main():
    import os

    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
    DATA_DIR = os.path.join(BASE_DIR, 'data', 'pa1-data')

    if not os.path.exists(DATA_DIR):
        print(f"Dataset not found at {DATA_DIR}. Please download and extract it.")
        return

    bsbi_base = BSBIIndex(
        data_dir=DATA_DIR, 
        output_dir=os.path.join(BASE_DIR, 'data', 'output_dir'), 
        index_name='BSBI_Base',
        postings_encoding=UncompressedPostings
    )
    setup_directories()
    
    print("Running basic unit tests...")
    test_id_map()
    test_compression()
    print("-" * 40)

    if not os.path.exists('data/pa1-data'):
        print("Dataset 'pa1-data' not found in current directory. Please download and extract it.")
        return

    # 无压缩 BSBI
    print("Starting Base BSBI Indexing (Uncompressed)...")
    bsbi_base = BSBIIndex(
        data_dir='data/pa1-data', 
        output_dir='data/output_dir', 
        index_name='BSBI_Base',
        postings_encoding=UncompressedPostings
    )
    bsbi_base.index()
    print("Testing Base BSBI Retrieval...")
    run_dev_queries(bsbi_base)
    print("-" * 40)
    
    # 变长字节编码
    print("Starting Compressed BSBI Indexing (Variable Byte Encoding)...")
    bsbi_compressed = BSBIIndex(
        data_dir='data/pa1-data', 
        output_dir='data/output_dir_compressed', 
        index_name='BSBI_Compressed',
        postings_encoding=CompressedPostings
    )
    bsbi_compressed.index()
    print("Testing Compressed BSBI Retrieval...")
    run_dev_queries(bsbi_compressed)
    print("-" * 40)
    
    # Elias Gamma 编码
    print("Starting Extra Compressed BSBI Indexing (Elias Gamma Encoding)...")
    bsbi_ec = BSBIIndex(
        data_dir='data/pa1-data', 
        output_dir='data/output_dir_ec', 
        index_name='BSBI_EC',
        postings_encoding=ECCompressedPostings
    )
    bsbi_ec.index()
    print("Testing Extra Compressed BSBI Retrieval...")
    run_dev_queries(bsbi_ec)
    print("=" * 40)
    print("All index building and query retrieval processes finished successfully.")

if __name__ == "__main__":
    main()