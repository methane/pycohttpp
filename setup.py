from distutils.core import setup, Extension

pycomod = Extension(
    'pycohttpp.parser',
    sources=['pycohttpp/parser.c',
             'picohttpparser/picohttpparser.c'],
    include_dirs=['./picohttpparser'],
)

setup(name='picohttpp',
      version='0.1.dev0',
      description='picohttpparser binding',
      packages=['pycohttpp'],
      ext_modules=[pycomod],
)
