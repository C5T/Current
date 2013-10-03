# This Makefile only confirms that the project can be built.

CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

# Build the headers individually to confirm they contain all the dependencies.
all: fncas_gcc fncas_clang fncas_jit_ok fncas.o fncas_base.o fncas_node.o fncas_jit.o

fncas_gcc: dummy.cc *.h
	g++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

fncas_clang: dummy.cc *.h
	clang++ ${CCFLAGS} -o $@ dummy.cc ${CCPOSTFLAGS}

fncas_jit_ok: dummy.cc *.h
	g++ -DFNCAS_JIT=NASM --std=c++0x -o /dev/null dummy.cc -ldl
	g++ -DFNCAS_JIT=CLANG --std=c++0x -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=NASM --std=c++0x -o /dev/null dummy.cc -ldl
	clang++ -DFNCAS_JIT=CLANG --std=c++0x -o /dev/null dummy.cc -ldl
	echo OK >$@

%.o: %.h
	g++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}
	clang++ ${CCFLAGS} -c -o $@ $< ${CCPOSTFLAGS}

clean:
	rm -f fncas fncas_gcc fncas_clang fncas_jit_ok *.o
