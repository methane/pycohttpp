# Benchmark

## Setup

This benchmark uses nginx with ngx-lua-module and ngx-echo.
If you use Mac and Homebrew, you can install it with:

```console
$ brew install openresty
```


## Run

Run nginx:

```console
$ openresty -c `pwd`/nginx.conf
```

Then, run benchmark:

```console
$ ./bench.py requests
1.0693879127502441
$ ./bench.py -p requests
0.9582479000091553
$ ./bench.py urllib3
0.5229451656341553
$ ./bench.py -p urllib3
0.4077160358428955
```

## Profile

You can use cProfile to profile it.

```console
$ python -m cProfile -scall ./bench.py -p urllib3
0.4734969139099121
         382785 function calls (380300 primitive calls) in 0.607 seconds

   Ordered by: call count

   ncalls  tottime  percall  cumtime  percall filename:lineno(function)
    36138    0.005    0.000    0.005    0.000 {method 'lower' of 'str' objects}
25821/25558    0.003    0.000    0.003    0.000 {built-in method len}
    21206    0.008    0.000    0.013    0.000 {built-in method isinstance}
    14024    0.005    0.000    0.005    0.000 {method 'encode' of 'str' objects}
    11215    0.002    0.000    0.002    0.000 {method 'append' of 'list' objects}
    10188    0.007    0.000    0.007    0.000 {built-in method hasattr}
     8000    0.003    0.000    0.016    0.000 _policybase.py:299(header_fetch_parse)
     8000    0.003    0.000    0.005    0.000 utils.py:51(_has_surrogates)
     8000    0.006    0.000    0.012    0.000 _policybase.py:269(_sanitize_header)
     6003    0.002    0.000    0.002    0.000 timeout.py:104(_validate_timeout)
```
