# This Makefile only confirms that the project can be built.
# Also builds headers individually to confirm they contain all the dependencies.
#
# The following warnings are OK -- D.K.
# clang: warning: treating 'c-header' input as 'c++-header' when in C++ mode, this behavior is deprecated
# clang: warning: -ldl: 'linker' input unused

include ../../scripts/Makefile

CCFLAGS=--std=c++11 -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: .current/fncas_gcc .current/fncas_clang .current/fncas_jit_ok .current/fncas.o .current/fncas_base.o .current/fncas_node.o .current/fncas_differentiate.o .current/fncas_jit.o

.current/fncas_gcc: dummy.cc *.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

.current/fncas_clang: dummy.cc *.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

.current/fncas_jit_ok: dummy.cc *.h
	g++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	g++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=NASM --std=c++11 -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=CLANG --std=c++11 -o /dev/null dummy.cc -ldl
	echo OK >$@

.current/%.o: %.h
	g++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
	clang++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
