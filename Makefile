.PHONY: build
build:
	python setup.py build_ext -if

.PHONY: test
test:
	pip install -e .
	py.test -v tests
