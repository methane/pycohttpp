.PHONY: build
build:
	python setup.py build_ext

.PHONY: test
test:
	pip install -e .
	py.test -v tests
