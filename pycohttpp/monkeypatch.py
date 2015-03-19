from .parser import parse_headers as c_parse_headers


_orig_http_client = None

def patch_http_client():
    """Monkey patch to http.client.parse_headers"""
    import http.client
    global _orig_http_client
    _orig_http_client = http.client.parse_headers

    def parse_headers(fp, _class=http.client.HTTPMessage):
        try:
            buf = b''
            while True:
                ch = fp.peek(80*1024 - len(buf))
                if not ch:
                    raise http.client.HTTPException('Incomplete request')
                buf += ch
                res = c_parse_headers(buf, 2, 1, 0)
                if res is not None:
                    n, hs = res
                    remains = len(buf) - n
                    fp.read(len(ch) - remains)
                    break
                fp.read(len(ch))
        except Exception as e:
            raise http.client.HTTPException(str(e))
        msg = _class()
        msg._headers = hs
        return msg

    parse_headers.__doc__ = http.client.parse_headers.__doc__

    http.client.parse_headers = parse_headers
