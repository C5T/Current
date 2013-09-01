CC=g++
CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: test

test: test_binary
	./test_binary
	rm -f test_binary

test_binary : test.cc autogen/functions.h
	${CC} ${CCFLAGS} -o test_binary test.cc ${CCPOSTFLAGS}

autogen/functions.h: functions/*.h
	mkdir -p autogen
	cat functions/*.h >$@
	ls functions/ | sed "s/\(.*\)\.h/REGISTER_FUNCTION(\1);/g" >>$@

clean:
	rm -rf autogen/
	rm -f test_binary
