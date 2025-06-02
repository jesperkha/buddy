CC = clang
FLAGS = -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -std=c99

all:
	mkdir -p bin
	$(CC) $(FLAGS) buddy.c test.c -o bin/test
	./bin/test

build:
	mkdir -p bin dist
	touch dist/buddy.h
	$(CC) $(FLAGS) buddy.c build.c -o bin/build
	./bin/build

gcc:
	mkdir -p bin
	gcc -Wall -Wextra -Wpedantic -Werror buddy.c test.c -o bin/test
	./bin/test

clean:
	rm -rf bin dist
	rm -f *.exe *.txt
