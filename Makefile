all:
	mkdir -p bin
	gcc test.c -o bin/test
	./bin/test

doc:
	python3 autodoc.py buddy.h
