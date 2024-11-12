all:
	mkdir -p bin
	gcc test/*.c -o bin/test
	./bin/test

doc:
	python autodoc.py buddy.h