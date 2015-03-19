#!/usr/bin/env python3
import functools
import time
import sys
import requests
from requests.packages import urllib3
import pycohttpp.monkeypatch


N = 1000
URL = 'http://127.0.0.1:8080/'
BODY = b'Hello, World!\n'


def timer(func):
    @functools.wraps(func)
    def timed():
        t0 = time.time()
        try:
            return func()
        finally:
            t1 = time.time()
            print(t1 - t0)
    return timed


@timer
def bench_requests():
    sess = requests.Session()
    sess.trust_env = False  # Skip detecting proxy. It's very slow.
    for _ in range(N):
        resp = sess.get(URL, stream=False)
        assert resp.content == BODY


@timer
def bench_urllib3():
    http = urllib3.PoolManager()
    for _ in range(N):
        r = http.request('GET', URL)
        assert r.status == 200
        assert r.data == BODY
    t1 = time.time()


def usage():
    sys.exit("Usage: ./bench.py [-p] requests|urllib3")


def main():
    args = sys.argv[1:]
    if not args: usage()

    if args[0] == '-p':
        pycohttpp.monkeypatch.patch_http_client()
        del args[0]

    if not args: usage()

    if args[0] == 'requests':
        bench_requests()
    elif args[0] == 'urllib3':
        bench_urllib3()
    else:
        usage()


main()
