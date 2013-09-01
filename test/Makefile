# Tested with clang++ as well.
CC=clang++
CCFLAGS=--std=c++0x -Wall -O3
CCPOSTFLAGS=-ldl

all: test

test: test_binary
	./test_binary
	rm -f test_binary

test_binary : test.cc
	${CC} ${CCFLAGS} -o test_binary test.cc ${CCPOSTFLAGS}

clean:
	rm -f test_binary
