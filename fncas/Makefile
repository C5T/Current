# This Makefile only confirms that the project can be built.

CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: fncas

fncas: fncas_gcc fncas_clang
	ln -sf fncas_clang fncas

fncas_gcc: dummy.cc fncas.h fncas_base.h fncas_node.h fncas_jit.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

fncas_clang: dummy.cc fncas.h fncas_base.h fncas_node.h fncas_jit.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

clean:
	rm -f fncas fncas_gcc fncas_clang
