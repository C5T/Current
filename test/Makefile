# Tested with clang++ as well.
CC=clang++
CCFLAGS=--std=c++0x -Wall -O3
CCPOSTFLAGS=-ldl

all: test

test: test.cc
	${CC} ${CCFLAGS} -o test_binary test.cc ${CCPOSTFLAGS}
	./test_binary

clean:
	rm -f test_binary
