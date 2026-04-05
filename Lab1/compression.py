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
                # 截取低 7 位数据
                chunk_stack.insert(0, gap & 0x7F)
                gap >>= 7
                if gap == 0:
                    break
                    
            # 最高位赋予 1 作为结束标识符
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
                # 遇到结束标识，合并最后 7 位并清算当前 Gap
                gap = (gap << 7) | (byte & 0x7F)
                last_doc_id += gap
                postings_list.append(last_doc_id)
                gap = 0
            else:
                # 未结束，继续累加 7 位数据
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
        last_doc = 0
        
        for doc_id in postings_list:
            gap = doc_id - last_doc
            last_doc = doc_id
            
            # Elias Gamma 编码核心逻辑
            # 将间距转为无前缀二进制字符串 (例如 13 -> '1101')
            bin_gap = bin(gap)[2:]
            
            # 剥离首位的 1，剩余部分为 offset
            offset = bin_gap[1:]
            
            # 构造一元码：长度为 offset 位数的连续 '1'，以 '0' 结尾
            unary = '1' * len(offset) + '0'
            
            bit_string += unary + offset
            
        # 字节对齐：计算需要补 0 的数量
        padding_len = (8 - len(bit_string) % 8) % 8
        bit_string += '0' * padding_len
        
        # 将二进制字符串每 8 位切割，转为整型并写入字节数组
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
            
        # 将字节流全部展开为连续的 0/1 字符串，保留前导零
        bit_string = "".join([f"{byte:08b}" for byte in encoded_postings_list])
        
        postings_list = []
        last_doc = 0
        
        idx = 0
        length = len(bit_string)
        
        while idx < length:
            # 1. 统计一元码长度（连续的 1）
            unary_count = 0
            while idx < length and bit_string[idx] == '1':
                unary_count += 1
                idx += 1
                
            # 若触发边界，或者当前位是 '0' 且后续全为对齐用的填充 '0'，则结束解码
            if idx >= length or (unary_count == 0 and bit_string[idx] == '0' and all(c == '0' for c in bit_string[idx:])):
                break
                
            # 2. 消耗一元码的结束符 '0'
            idx += 1
            
            # 3. 读取 offset 数据
            if idx + unary_count > length:
                break
                
            offset_bits = bit_string[idx:idx + unary_count]
            idx += unary_count
            
            # 4. 还原间距并累加绝对 ID
            # 还原公式：2^unary_count + offset_bits
            if unary_count > 0:
                gap = (1 << unary_count) + int(offset_bits, 2)
            else:
                # 特殊情况：Gap = 1 时，unary_count = 0，offset 为空
                gap = 1
                
            last_doc += gap
            postings_list.append(last_doc)
            
        return postings_list