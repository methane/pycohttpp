from distutils.core import setup, Extension

pycomod = Extension(
    'pycohttpp',
    sources=['pycohttpp.c', 'picohttpparser/picohttpparser.c'],
    include_dirs=['picohttpparser'],
)

setup(name='picohttpp',
      version='0.1.dev0',
      description='picohttpparser binding',
      ext_modules=[pycomod],
)
