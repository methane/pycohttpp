from pycohttpp.parser import parse_headers


simple_header = b"""\
Host: example.com\xB5\r
Content-Length: 10\r
\r
"""


def test_bytes_bytes():
    res = parse_headers(simple_header, 0, 0, 0)
    print(res)
    assert res is not None
    size, fields = res
    assert size == len(simple_header)
    assert fields == [
        (b'Host', b'example.com\xb5'),
        (b'Content-Length', b'10'),
    ]


def test_asis_latin1():
    res = parse_headers(simple_header, 1, 1, 0)
    print(res)
    assert res is not None
    size, fields = res
    assert size == len(simple_header)
    assert fields == [
        ('Host', 'example.com\xb5'),
        ('Content-Length', '10'),
    ]


def test_lower_ascii():
    res = parse_headers(simple_header, 2, 2, 0)
    print(res)
    assert res is not None
    size, fields = res
    assert size == len(simple_header)
    assert fields == [
        ('host', 'example.com\udcb5'),
        ('content-length', '10'),
    ]


def test_upper_ascii():
    res = parse_headers(simple_header, 3, 2, 0)
    print(res)
    assert res is not None
    size, fields = res
    assert size == len(simple_header)
    assert fields == [
        ('HOST', 'example.com\udcb5'),
        ('CONTENT-LENGTH', '10'),
    ]
