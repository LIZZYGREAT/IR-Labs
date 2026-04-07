import os
import pickle as pkl
from compression import UncompressedPostings

class InvertedIndex:
    """A class that implements efficient reads and writes of an inverted index 
    to disk
    """
    def __init__(self, index_name, postings_encoding=None, directory=''):
        self.index_file_path = os.path.join(directory, index_name+'.index')
        self.metadata_file_path = os.path.join(directory, index_name+'.dict')

        if postings_encoding is None:
            self.postings_encoding = UncompressedPostings
        else:
            self.postings_encoding = postings_encoding
        self.directory = directory

        self.postings_dict = {}
        self.terms = []         

    def __enter__(self):
        """Opens the index_file and loads metadata upon entering the context"""
        self.index_file = open(self.index_file_path, 'rb+')

        with open(self.metadata_file_path, 'rb') as f:
            self.postings_dict, self.terms = pkl.load(f)
            self.term_iter = self.terms.__iter__()                       

        return self
    
    def __exit__(self, exception_type, exception_value, traceback):
        """Closes the index_file and saves metadata upon exiting the context"""
        self.index_file.close()
        
        with open(self.metadata_file_path, 'wb') as f:
            pkl.dump([self.postings_dict, self.terms], f)


class InvertedIndexWriter(InvertedIndex):
    """"""
    def __enter__(self):
        self.index_file = open(self.index_file_path, 'wb+')              
        return self

    def append(self, term, postings_list):
        """Appends the term and postings_list to end of the index file."""
        encoded_bytes = self.postings_encoding.encode(postings_list)
        length_in_bytes = len(encoded_bytes)
        number_of_postings = len(postings_list)
        
        start_position = self.index_file.tell()
        
        self.postings_dict[term] = (start_position, number_of_postings, length_in_bytes)
        self.terms.append(term)
        
        self.index_file.write(encoded_bytes)


class InvertedIndexIterator(InvertedIndex):
    """"""
    def __enter__(self):
        """Adds an initialization_hook to the __enter__ function of super class
        """
        super().__enter__()
        self._initialization_hook()
        return self

    def __iter__(self): 
        return self
    
    def _initialization_hook(self):
        """Use this function to initialize the iterator"""
        self.index_file.seek(0)

    def __next__(self):
        """Returns the next (term, postings_list) pair in the index."""
        try:
            term = next(self.term_iter)
        except StopIteration:
            raise StopIteration
            
        metadata = self.postings_dict[term]
        length_in_bytes = metadata[2]
        
        encoded_bytes = self.index_file.read(length_in_bytes)
        
        postings_list = self.postings_encoding.decode(encoded_bytes)
        
        return (term, postings_list)

    def delete_from_disk(self):
        """Marks the index for deletion upon exit. Useful for temporary indices
        """
        self.delete_upon_exit = True

    def __exit__(self, exception_type, exception_value, traceback):
        """Delete the index file upon exiting the context along with the
        functions of the super class __exit__ function"""
        self.index_file.close()
        if hasattr(self, 'delete_upon_exit') and self.delete_upon_exit:
            os.remove(self.index_file_path)
            os.remove(self.metadata_file_path)
        else:
            with open(self.metadata_file_path, 'wb') as f:
                pkl.dump([self.postings_dict, self.terms], f)


class InvertedIndexMapper(InvertedIndex):
    def __getitem__(self, key):
        return self._get_postings_list(key)
    
    def _get_postings_list(self, term):
        """Gets a postings list (of docIds) for `term`.
        
        This function should not iterate through the index file.
        I.e., it should only have to read the bytes from the index file
        corresponding to the postings list for the requested term.
        """
        if term not in self.postings_dict:
            return []
            
        metadata = self.postings_dict[term]
        start_position = metadata[0]
        length_in_bytes = metadata[2]
        
        self.index_file.seek(start_position)
        
        encoded_bytes = self.index_file.read(length_in_bytes)

        return self.postings_encoding.decode(encoded_bytes)


def sorted_intersect(list1, list2):
    """Intersects two (ascending) sorted lists and returns the sorted result
    
    Parameters
    ----------
    list1: List[Comparable]
    list2: List[Comparable]
        Sorted lists to be intersected
        
    Returns
    -------
    List[Comparable]
        Sorted intersection        
    """
    intersection = []
    i, j = 0, 0
    
    while i < len(list1) and j < len(list2):
        if list1[i] == list2[j]:
            intersection.append(list1[i])
            i += 1
            j += 1
        elif list1[i] < list2[j]:
            i += 1
        else:
            j += 1
            
    return intersection