all: fncas

fncas: fncas.cc
	g++ -o fncas fncas.cc

clean:
	rm -f fncas
