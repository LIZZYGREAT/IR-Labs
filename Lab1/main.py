import os
from id_map import IdMap
from compression import UncompressedPostings, CompressedPostings, ECCompressedPostings
from index_io import InvertedIndexWriter, InvertedIndexIterator
from bsbi import BSBIIndex

def setup_directories():
    """初始化运行环境所需的目录结构"""
    directories = ['output_dir', 'tmp', 'toy_output_dir', 'output_dir_compressed']
    for d in directories:
        try:
            os.mkdir(d)
        except FileExistsError:
            pass

def test_id_map():
    """测试 IdMap 的基础双向映射功能"""
    testIdMap = IdMap()
    # TODO: 编写断言测试 _get_str 和 _get_id 的正确性与边界情况
    ### Begin your code
    pass
    ### End your code

def test_compression():
    """测试压缩模块的编解码一致性"""
    # TODO: 编写断言测试 CompressedPostings.encode 和 decode
    ### Begin your code
    pass
    ### End your code

def run_dev_queries(index_instance):
    """
    运行 dev_queries 下的测试集，并将检索结果与 dev_output 的标准答案比对
    """
    for i in range(1, 9):
        query_path = os.path.join('dev_queries', f'query.{i}')
        output_path = os.path.join('dev_output', f'{i}.out')
        
        with open(query_path, 'r') as q:
            query = q.read()
            # 获取检索结果并标准化路径
            my_results = [os.path.normpath(path) for path in index_instance.retrieve(query)]
            
            with open(output_path, 'r') as o:
                reference_results = [os.path.normpath(x.strip()) for x in o.readlines()]
                
                # TODO: 编写断言逻辑比对 my_results 和 reference_results，并在不匹配时输出差异
                ### Begin your code
                pass
                ### End your code
            print(f"Results match for query: {query.strip()}")

def main():
    # 1. 准备运行环境
    setup_directories()
    
    # 2. 执行基础组件单元测试
    test_id_map()
    test_compression()
    
    # 3. 实例化核心引擎，并注入所选的压缩策略（此处以无压缩基线为例）
    print("Starting Base BSBI Indexing...")
    bsbi_instance = BSBIIndex(
        data_dir='pa1-data', 
        output_dir='output_dir', 
        postings_encoding=UncompressedPostings
    )
    
    # 4. 执行全量索引构建（如果尚未构建）
    # bsbi_instance.index()
    
    # 5. 执行联合布尔检索测试
    print("Testing Base BSBI Retrieval...")
    # run_dev_queries(bsbi_instance)
    
    # TODO: 当压缩模块开发完毕后，可在此处另起一个使用 CompressedPostings 的实例并测试
    ### Begin your code
    pass
    ### End your code

if __name__ == "__main__":
    main()