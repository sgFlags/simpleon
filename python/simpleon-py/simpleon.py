import re
import struct

def default_token_handler(s):
    if s[0] in "-0123456789.":
        try:
            return int(s)
        except ValueError:
            pass

        try:
            return float(s)
        except ValueError:
            pass

    if s[0] == "t" and s == "true":
        return True

    if s[0] == "f" and s == "false":
        return False
    
    if s[0] == "n" and s == "null":
        return None

    return s

class SimpleONParser:

    STATE_ELEMENT_START    = 0
    STATE_ELEMENT_END      = 1
    STATE_DICT_PRE_KEY     = 2
    STATE_DICT_KEY         = 3
    STATE_DICT_POST_KEY    = 4
    STATE_DICT_VALUE       = 5
    STATE_DICT_POST_VALUE  = 6
    STATE_LIST             = 7
    STATE_QUOTED_STRING    = 8
    STATE_QUOTELESS_STRING = 9
    STATE_MULTILINE_STRING = 10

    # FOR DEBUGGING:
    # STATE_ELEMENT_START    = "ES"
    # STATE_ELEMENT_END      = "EE"
    # STATE_DICT_PRE_KEY     = "K-"
    # STATE_DICT_KEY         = "K:"
    # STATE_DICT_POST_KEY    = "K+"
    # STATE_DICT_VALUE       = "V:"
    # STATE_DICT_POST_VALUE  = "V+"
    # STATE_LIST             = "L"
    # STATE_QUOTED_STRING    = "QS"
    # STATE_QUOTELESS_STRING = "QL"
    # STATE_MULTILINE_STRING = "ML"

    COMMENT_CHAR_RE = re.compile(r'#')
    QUOTED_STRING_SPECIAL_RE = re.compile(r'[\\"]')
    MULTILINE_STRING_SPECIAL_RE = re.compile(r'\\|"""')
    UNQUOTED_RE = re.compile(r'[^\[\]{}:", \t]+')
    DICT_KEY_VALUE_SEP_RE = re.compile(":")
    ITEM_SEP_RE = re.compile(",")
    NON_WHITESPACE_RE = re.compile(r'[^ \t]')

    BUF_CLEAN_SIZE = 4096
    
    def __init__(self, token_handler = default_token_handler):
        self.state_stack = [ self.STATE_ELEMENT_START ];
        self.value_stack = [ None ];
        self.buf = ""
        self.buf_read_pos = 0
        self.token_handler = token_handler
        pass

    def state_get(self):
        if len(self.state_stack) == 0:
            raise Exception("stack is empty")
        return self.state_stack[len(self.state_stack) - 1]

    def state_set(self, state):
        if len(self.state_stack) == 0:
            raise Exception("stack is empty")
        self.state_stack[len(self.state_stack) - 1] = state

    def value_get(self):
        if len(self.value_stack) == 0:
            raise Exception("stack is empty")
        return self.value_stack[len(self.value_stack) - 1]

    def value_set(self, value):
        if len(self.value_stack) == 0:
            raise Exception("stack is empty")
        self.value_stack[len(self.value_stack) - 1] = value

    def pop(self):
        if len(self.state_stack) == 0:
            raise Exception("stack is empty")
        self.state_stack.pop()
        self.value_stack.pop()

    def push(self, state, value):
        if len(self.state_stack) == 0:
            raise Exception("stack is empty")
        self.state_stack.append(state)
        self.value_stack.append(value)

    def parse(self, s):
        self.parse_lines(s.splitlines())

    def parse_lines(self, lines):
        for line in lines:
            self.parse_line(line)
        pass

    def parse_line(self, line):
        self.buf += line
        self.parse_buf()
        
    def handle_escape(self):
        start_c = self.buf[self.buf_read_pos]
        if start_c == 'n':
            self.value_get().extend(b'\n')
            self.buf_read_pos += 1
        elif start_c == 't':
            self.value_get().extend(b'\t')
            self.buf_read_pos += 1
        elif start_c == 'b':
            self.value_get().extend(b'\b')
            self.buf_read_pos += 1
        elif start_c == 'f':
            self.value_get().extend(b'\f')
            self.buf_read_pos += 1
        elif start_c == 'x':
            if self.buf_read_pos + 2 >= len(self.buf):
                raise Exception("expect 2 hex chars for utf-8 escaping")
            code_str = self.buf[self.buf_read_pos + 1:self.buf_read_pos + 3]
            try:
                code = int(code_str, 16)
            except Exception:
                raise Exception("expect 2 hex chars for utf-8 escaping (parse {0} failed)".format(code_str))
            self.value_get().append(code)
            self.buf_read_pos += 3
        elif start_c == '"' or start_c == '\\' or start_c == '/':
            self.value_get().append(ord(start_c))
            self.buf_read_pos += 1
        else:
            self.value_get().extend(b"\\")
            self.value_get().append(ord(start_c))
        
    def parse_buf(self):
        while len(self.buf) > self.buf_read_pos:

            if self.buf_read_pos > self.BUF_CLEAN_SIZE:
                self.buf = self.buf[self.buf_read_pos:]
                self.buf_read_pos = 0

            state = self.state_get()
            current = self.value_get()
            read_pos = self.buf_read_pos
            # print((state, current, read_pos))

            if state == self.STATE_ELEMENT_END:
                value = current
                if isinstance(value, bytearray):
                    value = value.decode("utf-8")
                self.pop()

                state = self.state_get()
                current = self.value_get()

                if state == self.STATE_DICT_KEY:
                    state = self.STATE_DICT_POST_KEY
                    current = [current, value]
                elif state == self.STATE_DICT_VALUE:
                    current[0][current[1]] = value
                    state = self.STATE_DICT_POST_VALUE
                    current = current[0]
                elif state == self.STATE_LIST:
                    current.append(value)
                else:
                    raise Exception("invalid position to insert element")
                
            elif state == self.STATE_QUOTED_STRING:
                m = self.QUOTED_STRING_SPECIAL_RE.search(self.buf, read_pos)
                if not m:
                    current.extend(self.buf[read_pos:].encode("utf-8"))
                    state = self.STATE_ELEMENT_END
                    read_pos = len(self.buf)
                else:
                    current.extend(self.buf[read_pos:m.start(0)].encode("utf-8"))
                    if self.buf[m.start(0)] == "\\":
                        self.buf_read_pos = m.start(0) + 1
                        self.handle_escape()
                        continue
                    elif self.buf[m.start(0)] == '"':
                        # the string is ended
                        read_pos = m.start(0) + 1 
                        state = self.STATE_ELEMENT_END
                    else:
                        raise Exception("format error in quoted string")

            elif state == self.STATE_MULTILINE_STRING:
                m = self.MULTILINE_STRING_SPECIAL_RE.search(self.buf, read_pos)
                if not m:
                    current.extend(self.buf[read_pos:].encode("utf-8"))
                    current.extend(b"\n")
                    read_pos = len(self.buf)
                else:
                    current.extend(self.buf[read_pos:m.start(0)].encode("utf-8"))
                    if self.buf[m.start(0)] == "\\":
                        self.buf_read_pos = m.start(0) + 1
                        self.handle_escape()
                        continue
                    elif self.buf[m.start(0):m.start(0) + 3] == '"""':
                        # the string is ended
                        read_pos = m.start(0) + 3
                        state = self.STATE_ELEMENT_END
                    else:
                        raise Exception("format error in multi-line string")

            elif state == self.STATE_DICT_PRE_KEY:
                m = self.NON_WHITESPACE_RE.search(self.buf, read_pos)
                if not m:
                    self.buf_read_pos = len(self.buf)
                elif self.buf[m.start(0)] == '"' or self.UNQUOTED_RE.match(self.buf[m.start(0)]):
                    self.buf_read_pos = m.start(0)
                    self.state_set(self.STATE_DICT_KEY);
                    self.push(self.STATE_ELEMENT_START, None);
                    continue
                elif self.buf[m.start(0)] == "}":
                    state = self.STATE_ELEMENT_END
                    read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == "#":
                    read_pos = len(self.buf)
                else:
                    raise Exception("format error - expecting dict key or end")

            elif state == self.STATE_DICT_KEY:
                pass

            elif state == self.STATE_DICT_POST_KEY:
                m = self.NON_WHITESPACE_RE.search(self.buf, read_pos)
                if not m:
                    read_pos = len(self.buf)
                elif self.DICT_KEY_VALUE_SEP_RE.match(self.buf[m.start(0)]):
                    self.buf_read_pos = m.start(0) + 1
                    self.state_set(self.STATE_DICT_VALUE)
                    self.push(self.STATE_ELEMENT_START, None)
                    continue
                elif self.buf[m.start(0)] == "#":
                    read_pos = len(self.buf)                
                else:
                    raise Exception("format error - expecting key-value-separator")
            
            elif state == self.STATE_DICT_VALUE:
                pass

            elif state == self.STATE_DICT_POST_VALUE:
                m = self.NON_WHITESPACE_RE.search(self.buf, read_pos)
                if not m:
                    read_pos = len(self.buf)
                elif self.buf[m.start(0)] == '"' or self.UNQUOTED_RE.match(self.buf[m.start(0)]):
                    state = self.STATE_DICT_PRE_KEY
                    read_pos = m.start(0)
                elif self.ITEM_SEP_RE.match(self.buf[m.start(0)]):
                    state = self.STATE_DICT_PRE_KEY
                    read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == "}":
                    state = self.STATE_ELEMENT_END
                    read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == "#":
                    read_pos = len(self.buf)
                else:
                    raise Exception("format error in dict")

            elif state == self.STATE_LIST:
                m = self.NON_WHITESPACE_RE.search(self.buf, read_pos)
                if not m:
                    read_pos = len(self.buf)
                elif self.buf[m.start(0)] == "]":
                    state = self.STATE_ELEMENT_END
                    read_pos = m.start(0) + 1
                elif self.ITEM_SEP_RE.match(self.buf[m.start(0)]):
                    self.buf_read_pos = m.start(0) + 1
                    self.push(self.STATE_ELEMENT_START, None)
                    continue
                elif self.buf[m.start(0)] == "#":
                    read_pos = len(self.buf)
                else:
                    self.buf_read_pos = m.start(0)
                    self.push(self.STATE_ELEMENT_START, None)
                    continue

            elif state == self.STATE_ELEMENT_START:
                m = self.NON_WHITESPACE_RE.search(self.buf, read_pos)
                if not m:
                    self.buf_read_pos = len(self.buf)
                elif self.buf[m.start(0)] == "{":
                    current = dict()
                    state = self.STATE_DICT_PRE_KEY
                    read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == "[":
                    current = list()
                    state = self.STATE_LIST
                    read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == '"':
                    current = bytearray()
                    if self.buf[m.start(0):m.start(0) + 3] == '"""':
                        state = self.STATE_MULTILINE_STRING
                        read_pos = m.start(0) + 3
                    else:
                        state = self.STATE_QUOTED_STRING
                        read_pos = m.start(0) + 1
                elif self.buf[m.start(0)] == "#":
                    read_pos = len(self.buf)
                elif self.UNQUOTED_RE.match(self.buf[m.start(0)]):
                    m_end = self.UNQUOTED_RE.search(self.buf, m.start(0))
                    if not m_end:
                        current = self.buf[m.start(0):]
                        state = self.STATE_ELEMENT_END
                        read_pos = len(self.buf)
                    else:
                        current = self.buf[m_end.start(0):m_end.end(0)]
                        state = self.STATE_ELEMENT_END
                        read_pos = m_end.end(0)
                    current = self.token_handler(current)
                else:
                    raise Exception("format error in parsing general element")

            self.state_set(state)
            self.value_set(current)
            self.buf_read_pos = read_pos
        
    def extract(self):
        value = None
        if len(self.state_stack) == 1:
            value = self.value_stack[0]

        self.state_stack = []
        self.value_stack = []
        self.buf = ""
        self.buf_read_pos = 0

        return value
