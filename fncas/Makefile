# This Makefile only confirms that the project can be built.

CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

# Build the headers individually to confirm they contain all the dependencies.
all: fncas fncas.o fncas_base.h fncas_node.o fncas_jit.o

fncas: fncas_gcc fncas_clang
	ln -sf fncas_clang fncas

%.o: %.h
	g++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
	clang++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}

fncas_gcc: dummy.cc *.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGSsrc}

fncas_clang: dummy.cc *.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

clean:
	rm -f fncas fncas_gcc fncas_clang *.o
