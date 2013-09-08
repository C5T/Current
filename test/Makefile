CC=g++
CCFLAGS=--std=c++0x -Wall -O3 -fno-strict-aliasing
CCPOSTFLAGS=-ldl

all: report.html

report.html: run_test.sh test_binary
	./run_test.sh | tee $@

test_binary : test.cc autogen/functions.h ../fncas/*.h
	${CC} ${CCFLAGS} -o test_binary test.cc ${CCPOSTFLAGS}

autogen/functions.h: functions/*.h
	mkdir -p autogen
	cat functions/*.h >$@
	ls functions/ | sed "s/\(.*\)\.h/REGISTER_FUNCTION(\1);/g" >>$@

clean:
	rm -rf autogen/
	rm -f test_binary report.html
