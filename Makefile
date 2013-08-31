# Tested with clang++ as well.
CC=clang++
CCFLAGS=--std=c++0x -Wall -O3
CCPOSTFLAGS=-ldl

all: fncas test

fncas: fncas.cc fncas.h
	${CC} ${CCFLAGS} -o fncas fncas.cc ${CCPOSTFLAGS}

test: test.cc fncas.h
	${CC} ${CCFLAGS} -o test test.cc ${CCPOSTFLAGS}
	./test

clean:
	rm -f fncas test
