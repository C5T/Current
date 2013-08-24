all: fncas

fncas: fncas.cc
	g++ --std=c++0x -o fncas fncas.cc

clean:
	rm -f fncas
