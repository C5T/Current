all: fncas

fncas: fncas.cc
	g++ --std=c++0x -o fncas fncas.cc

test: test.cc
	g++ --std=c++0x -o test test.cc
	./test

clean:
	rm -f fncas test
