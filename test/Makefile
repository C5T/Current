CC=g++
CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: test

test: test_binary
	./test_binary
	rm -f test_binary

test_binary : test.cc autogen/all_functions.h autogen/all_functions_list.h
	${CC} ${CCFLAGS} -o test_binary test.cc ${CCPOSTFLAGS}

autogen/all_functions.h: functions/*.h
	mkdir -p autogen
	cat functions/*.h >autogen/all_functions.h

autogen/all_functions_list.h: functions/*.h
	ls functions/ | sed "s/\.h//g"  | paste -s -d, >autogen/all_functions_list.h

clean:
	rm -rf autogen/
	rm -f test_binary
