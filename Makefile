# Tested with clang++ as well.

CC=g++
CCFLAGS=--std=c++0x -Wall

all: fncas test

fncas: fncas.cc fncas.h
	${CC} ${CCFLAGS} -o fncas fncas.cc

test: test.cc fncas.h
	${CC} ${CCFLAGS} -o test test.cc
	./test

clean:
	rm -f fncas test
