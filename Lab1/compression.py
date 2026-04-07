import array

class UncompressedPostings:
    
    @staticmethod
    def encode(postings_list):
        """Encodes postings_list into a stream of bytes
        
        Parameters
        ----------
        postings_list: List[int]
            List of docIDs (postings)
            
        Returns
        -------
        bytes
            bytearray representing integers in the postings_list
        """
        return array.array('L', postings_list).tobytes()
        
    @staticmethod
    def decode(encoded_postings_list):
        """Decodes postings_list from a stream of bytes
        
        Parameters
        ----------
        encoded_postings_list: bytes
            bytearray representing encoded postings list as output by encode 
            function
            
        Returns
        -------
        List[int]
            Decoded list of docIDs from encoded_postings_list
        """
        decoded_postings_list = array.array('L')
        decoded_postings_list.frombytes(encoded_postings_list)
        return decoded_postings_list.tolist()


class CompressedPostings:
    
    @staticmethod
    def encode(postings_list):
        """Encodes `postings_list` using gap encoding with variable byte 
        encoding for each gap
        
        Parameters
        ----------
        postings_list: List[int]
            The postings list to be encoded
        
        Returns
        -------
        bytes: 
            Bytes reprsentation of the compressed postings list 
            (as produced by `array.tobytes` function)
        """
        encoded_bytes = bytearray()
        last_doc_id = 0
        
        for doc_id in postings_list:
            gap = doc_id - last_doc_id
            last_doc_id = doc_id
            
            chunk_stack = []
            while True:
                chunk_stack.insert(0, gap & 0x7F)
                gap >>= 7
                if gap == 0:
                    break
                    
            chunk_stack[-1] |= 0x80
            
            encoded_bytes.extend(chunk_stack)
            
        return bytes(encoded_bytes)

        
    @staticmethod
    def decode(encoded_postings_list):
        """Decodes a byte representation of compressed postings list
        
        Parameters
        ----------
        encoded_postings_list: bytes
            Bytes representation as produced by `CompressedPostings.encode` 
            
        Returns
        -------
        List[int]
            Decoded postings list (each posting is a docIds)
        """
        postings_list = []
        last_doc_id = 0
        gap = 0
        
        for byte in encoded_postings_list:
            if byte & 0x80:
                gap = (gap << 7) | (byte & 0x7F)
                last_doc_id += gap
                postings_list.append(last_doc_id)
                gap = 0
            else:
                gap = (gap << 7) | byte
                
        return postings_list


class ECCompressedPostings:
    
    @staticmethod
    def encode(postings_list):
        """Encodes `postings_list` 
        
        Parameters
        ----------
        postings_list: List[int]
            The postings list to be encoded
        
        Returns
        -------
        bytes: 
            Bytes reprsentation of the compressed postings list 
        """
        if not postings_list:
            return b""
            
        bit_string = ""
        last_doc = -1
        
        for doc_id in postings_list:
            gap = doc_id - last_doc
            last_doc = doc_id
            

            bin_gap = bin(gap)[2:]
            
            offset = bin_gap[1:]
            
            unary = '1' * len(offset) + '0'
            
            bit_string += unary + offset
            
        padding_len = (8 - len(bit_string) % 8) % 8
        bit_string += '1' * padding_len
        
        byte_arr = bytearray()
        for i in range(0, len(bit_string), 8):
            byte_arr.append(int(bit_string[i:i+8], 2))
            
        return bytes(byte_arr)

        
    @staticmethod
    def decode(encoded_postings_list):
        """Decodes a byte representation of compressed postings list
        
        Parameters
        ----------
        encoded_postings_list: bytes
            Bytes representation as produced by `CompressedPostings.encode` 
            
        Returns
        -------
        List[int]
            Decoded postings list (each posting is a docId)
        """
        if not encoded_postings_list:
            return []
            
        bit_string = "".join([f"{byte:08b}" for byte in encoded_postings_list])
        postings_list = []
        last_doc = -1

        idx = 0
        length = len(bit_string)
        while idx < length:
            unary_count = 0
            while idx < length and bit_string[idx] == '1':
                unary_count += 1
                idx += 1   
            if idx >= length:
                break
            idx += 1

            if idx + unary_count > length:
                break
            
            offset_bits = bit_string[idx:idx + unary_count]
            idx += unary_count
            
            if unary_count > 0:
                gap = (1 << unary_count) + int(offset_bits, 2)
            else:
                gap = 1
                
            last_doc += gap
            postings_list.append(last_doc)
            
        return postings_list