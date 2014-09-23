# This Makefile only confirms that the project can be built.
# Also builds headers individually to confirm they contain all the dependencies.
#
# The following warnings are OK -- D.K.
# clang: warning: treating 'c-header' input as 'c++-header' when in C++ mode, this behavior is deprecated
# clang: warning: -ldl: 'linker' input unused

CCFLAGS=--std=c++11 -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: fncas_gcc fncas_clang fncas_jit_ok fncas.o fncas_base.o fncas_node.o fncas_jit.o

fncas_gcc: dummy.cc *.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

fncas_clang: dummy.cc *.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

fncas_jit_ok: dummy.cc *.h
	g++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	g++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	echo OK >$@

%.o: %.h
	g++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
	clang++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}

clean:
	rm -f fncas fncas_gcc fncas_clang fncas_jit_ok *.o
