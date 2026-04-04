import os
import pickle as pkl
import contextlib
import heapq
from id_map import IdMap
from index_io import InvertedIndexWriter, InvertedIndexIterator

class BSBIIndex:
    """
    BSBI 倒排索引算法核心调度器
    """
    def __init__(self, data_dir, output_dir, index_name="BSBI", postings_encoding=None):
        self.term_id_map = IdMap()
        self.doc_id_map = IdMap()
        self.data_dir = data_dir
        self.output_dir = output_dir
        self.index_name = index_name
        self.postings_encoding = postings_encoding

        # Stores names of intermediate indices
        self.intermediate_indices = []

    def save(self):
        """Dumps doc_id_map and term_id_map into output directory"""
        with open(os.path.join(self.output_dir, 'terms.dict'), 'wb') as f:
            pkl.dump(self.term_id_map, f)
        with open(os.path.join(self.output_dir, 'docs.dict'), 'wb') as f:
            pkl.dump(self.doc_id_map, f)

    def load(self):
        """Loads doc_id_map and term_id_map from output directory"""
        with open(os.path.join(self.output_dir, 'terms.dict'), 'rb') as f:
            self.term_id_map = pkl.load(f)
        with open(os.path.join(self.output_dir, 'docs.dict'), 'rb') as f:
            self.doc_id_map = pkl.load(f)

    def index(self):
        """Base indexing code
        
        This function loops through the data directories, 
        calls parse_block to parse the documents
        calls invert_write, which inverts each block and writes to a new index
        then saves the id maps and calls merge on the intermediate indices
        """
        for block_dir_relative in sorted(next(os.walk(self.data_dir))[1]):
            td_pairs = self.parse_block(block_dir_relative)
            index_id = 'index_' + block_dir_relative
            self.intermediate_indices.append(index_id)
            with InvertedIndexWriter(index_id, directory=self.output_dir, 
                                     postings_encoding=self.postings_encoding) as index:
                self.invert_write(td_pairs, index)
                td_pairs = None
        self.save()
        with InvertedIndexWriter(self.index_name, directory=self.output_dir, 
                                 postings_encoding=self.postings_encoding) as merged_index:
            with contextlib.ExitStack() as stack:
                indices = [stack.enter_context(
                    InvertedIndexIterator(index_id, 
                                          directory=self.output_dir, 
                                          postings_encoding=self.postings_encoding)) 
                for index_id in self.intermediate_indices]
                self.merge(indices, merged_index)

    def parse_block(self, block_dir_relative):
        """Parses a tokenized text file into termID-docID pairs"""
        td_pairs = []
        block_path = os.path.join(self.data_dir, block_dir_relative)
        
        for filename in os.listdir(block_path):
            file_path = os.path.join(block_path, filename)
            
            # TODO_1: 构造 doc_relative_path (例如 "0/filename")
            ### Begin your code
            doc_relative_path = ""
            ### End your code
            
            doc_id = self.doc_id_map[doc_relative_path]
            
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                tokens = content.split()
                
                for token in tokens:
                    # TODO_2: 利用 self.term_id_map 获取 term_id
                    ### Begin your code
                    term_id = -1
                    ### End your code
                    
                    td_pairs.append((term_id, doc_id))
                    
        return td_pairs

    def invert_write(self, td_pairs, index):
        """Inverts td_pairs into postings_lists and writes them to the given index"""
        if not td_pairs:
            return
            
        # 1. 原址排序 (以 termID 为主，docID 为辅)
        td_pairs.sort()
        
        # 2. 状态机寄存器初始化
        current_term = None
        current_postings = []
        
        # 3. 线性扫描与聚合
        for term_id, doc_id in td_pairs:
            if term_id != current_term:
                if current_term is not None:
                    # TODO_3: 状态边界翻转，将当前收集完毕的倒排表落盘
                    ### Begin your code
                    pass
                    ### End your code
                    
                current_term = term_id
                current_postings = [doc_id]
            else:
                # TODO_4: 保持追踪态，O(1) 去重逻辑，确保递增且不重复
                ### Begin your code
                pass
                ### End your code
                
        # 4. 终态清理 (EOF)
        if current_term is not None:
            # TODO_5: 补全最后一次强制落盘逻辑
            ### Begin your code
            pass
            ### End your code

    def merge(self, indices, merged_index):
        """Merges multiple inverted indices into a single index"""
        # TODO_6: 实现外部多路归并逻辑
        # 提示：利用 heapq.merge 和 InvertedIndexIterator 的特性
        ### Begin your code
        pass
        ### End your code

    def retrieve(self, query):
        """Retrieves the documents corresponding to the conjunctive query"""
        if len(self.term_id_map) == 0 or len(self.doc_id_map) == 0:
            self.load()
            
        # TODO_7: 布尔交集检索逻辑
        # 提示：获取 query tokens 对应的倒排表 -> sorted_intersect 合并 -> 将 docID 映射为路径列表
        ### Begin your code
        pass
        ### End your code