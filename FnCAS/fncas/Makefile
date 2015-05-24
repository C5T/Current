# This Makefile only confirms that the project can be built.
# Also builds headers individually to confirm they contain all the dependencies.
#
# The following warnings are OK -- D.K.
# clang: warning: treating 'c-header' input as 'c++-header' when in C++ mode, this behavior is deprecated
# clang: warning: -ldl: 'linker' input unused

include ../KnowSheet/scripts/Makefile

CCFLAGS=--std=c++11 -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: .noshit/fncas_gcc .noshit/fncas_clang .noshit/fncas_jit_ok .noshit/fncas.o .noshit/fncas_base.o .noshit/fncas_node.o .noshit/fncas_differentiate.o .noshit/fncas_jit.o

.noshit/fncas_gcc: dummy.cc *.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

.noshit/fncas_clang: dummy.cc *.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

.noshit/fncas_jit_ok: dummy.cc *.h
	g++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	g++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	echo OK >$@

.noshit/%.o: %.h
	g++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
	clang++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
