CC=g++

fncas: fncas.cc fncas.h
	${CC} --std=c++0x -o fncas fncas.cc

all: fncas test

test: test.cc fncas.h
	${CC} --std=c++0x -o test test.cc
	./test

clean:
	rm -f fncas test
