# Tested with clang++ as well.
CC=clang++
CCFLAGS=--std=c++0x -Wall -O3
CCPOSTFLAGS=-ldl

all: test

test: test.cc
	${CC} ${CCFLAGS} -o test test.cc ${CCPOSTFLAGS}
	./test

clean:
	rm -f test
