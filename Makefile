# Run tests
all:
	mkdir -p bin
	gcc -Wall -Werror -Wpedantic -std=c99 test.c buddy.c -o bin/test
	./bin/test
