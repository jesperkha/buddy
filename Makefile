# Run tests
all:
	mkdir -p bin
	clang -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wunreachable-code buddy.c test.c -o bin/test 
	# clang -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wunreachable-code -fsanitize=undefined -fsanitize=address buddy.c test.c -o bin/test
	./bin/test
