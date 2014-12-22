.PHONY: all check clean

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -Wall
LDFLAGS=-pthread

PWD=$(shell pwd)

all:
	echo "TODO(dkorolev): Add tests for bricks/time."

check: build/CHECK_OK

build/CHECK_OK: build *.h
	for i in *.h ; do \
		echo -n $(basename $$i)': ' ; \
		ln -sf ${PWD}/$$i ${PWD}/build/$$i.cc ; \
		if [ ! -f build/$$i.h.o -o build/$$i.h.cc -nt build/$$i.h.o ] ; then \
			${CPLUSPLUS} -I . ${CPPFLAGS} -c build/$$i.cc -o build/$$i.h.o ${LDFLAGS} || exit 1 ; echo 'OK' ; \
		else \
			echo 'Already OK' ; \
		fi \
	done && echo OK >$@

build:
	mkdir -p $@

clean:
