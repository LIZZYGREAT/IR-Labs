class IdMap:
    """Helper class to store a mapping from strings to ids."""
    def __init__(self):
        self.str_to_id = {}
        self.id_to_str = []
        
    def __len__(self):
        """Return number of terms stored in the IdMap"""
        return len(self.id_to_str)
        
    def _get_str(self, i):
        """Returns the string corresponding to a given id (`i`)."""
        # 实现 O(1) 的 ID 查询 String 逻辑
        return self.id_to_str[i]
        
    def _get_id(self, s):
        """Returns the id corresponding to a string (`s`). 
        If `s` is not in the IdMap yet, then assigns a new id and returns the new id.
        """
        # 实现 String 查询 ID 逻辑，处理新字符串分配自增 ID 的情况
        if s not in self.str_to_id:
            new_id = len(self.id_to_str)
            self.str_to_id[s] = new_id
            self.id_to_str.append(s)
        return self.str_to_id[s]
            
    def __getitem__(self, key):
        """If `key` is a integer, use _get_str; 
           If `key` is a string, use _get_id;"""
        if type(key) is int:
            return self._get_str(key)
        elif type(key) is str:
            return self._get_id(key)
        else:
            raise TypeError