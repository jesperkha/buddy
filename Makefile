all:
	mkdir -p bin
	gcc -Wall -Werror -Wpedantic -std=c99 test.c -o bin/test
	./bin/test

doc:
	python3 autodoc.py buddy.h
