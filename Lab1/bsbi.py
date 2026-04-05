import os
import pickle as pkl
import contextlib
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
            
            doc_relative_path = os.path.join(block_dir_relative, filename)
            
            doc_id = self.doc_id_map[doc_relative_path]
            
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                tokens = content.split()
                
                for token in tokens:
                    term_id = self.term_id_map[token]
                    td_pairs.append((term_id, doc_id))
                    
        return td_pairs

    def invert_write(self, td_pairs, index):
        """Inverts td_pairs into postings_lists and writes them to the given index"""
        if not td_pairs:
            return
            
        td_pairs.sort()
        
        current_term = None
        current_postings = []
        
        for term_id, doc_id in td_pairs:
            if term_id != current_term:
                if current_term is not None:
                    #落盘旧的term
                    index.append(current_term, current_postings)
                
                #更新新的current_term
                current_term = term_id
                current_postings = [doc_id]
            else:
                if not current_postings or current_postings[-1] != doc_id:
                    #去重
                    current_postings.append(doc_id)
                
        if current_term is not None:
            index.append(current_term, current_postings)

    def merge(self, indices, merged_index):
        """Merges multiple inverted indices into a single index
        
        Parameters
        ----------
        indices: List[InvertedIndexIterator]
            A list of InvertedIndexIterator objects, each representing an
            iterable inverted index for a block
        merged_index: InvertedIndexWriter
            An instance of InvertedIndexWriter object into which each merged 
            postings list is written out one at a time
        """
        import heapq
        
        current_term = None
        current_postings = []
        
        merged_iter = heapq.merge(*indices, key=lambda x: x[0])  
        #参数为x，即posting_list 比较的值为x[0]即可，x内部天然有序
        
        for term_id, postings_list in merged_iter:
            if term_id != current_term:
                if current_term is not None:
                    #落盘旧的term
                    merged_index.append(current_term, current_postings)
                #更新新的current_term
                current_term = term_id
                current_postings = postings_list

            else:
                #天然有序性，直接extend即可
                current_postings.extend(postings_list)
                
        if current_term is not None:
            merged_index.append(current_term, current_postings)

    def retrieve(self, query):
        """Retrieves the documents corresponding to the conjunctive query
        
        Parameters
        ----------
        query: str
            Space separated list of query tokens
            
        Result
        ------
        List[str]
            Sorted list of documents which contains each of the query tokens. 
            Should be empty if no documents are found.
        
        Should NOT throw errors for terms not in corpus
        """
        if len(self.term_id_map) == 0 or len(self.doc_id_map) == 0:
            self.load()

        tokens = query.split()
        if not tokens:
            return []

        postings_lists = []
        
        with InvertedIndexMapper(self.index_name, directory=self.output_dir, postings_encoding=self.postings_encoding) as mapper:
            for token in tokens:
                # 关键!!必须要先查有没有，而不能直接哈希读取
                # 由于hash表会在不存在的情况下直接插入新的key，导致污染
                if token not in self.term_id_map.str_to_id:
                    return []
                    
                term_id = self.term_id_map.str_to_id[token]
                postings = mapper[term_id]
                postings_lists.append(postings)

        postings_lists.sort(key=len)

        result_doc_ids = postings_lists[0]
        for i in range(1, len(postings_lists)):
            result_doc_ids = sorted_intersect(result_doc_ids, postings_lists[i])
            if not result_doc_ids:
                return []

        result_paths = []
        for doc_id in result_doc_ids:
            doc_path = self.doc_id_map[doc_id]
            result_paths.append(doc_path)
            
        return result_paths