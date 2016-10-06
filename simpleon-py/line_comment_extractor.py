class LineCommentExtractor:
    def __init__(self, comment_re, inner_parser):
        self.comment_re = comment_re
        self.inner_parser = inner_parser

    def parse(self, s):
        self.parse_lines(s.splitlines())

    def parse_lines(self, lines):
        for line in lines:
            self.parse_line(line)
        pass

    def parse_line(self, line):
        m =  self.comment_re.search(line)
        if m:
            self.inner_parser.parse_line(line[m.end(0):])

    def extract(self):
        return self.inner_parser.extract()
