.PHONY: test all indent check clean

CPP = g++
CPPFLAGS = -std=c++11 -Wall -W
LDFLAGS = -pthread

SRC=$(wildcard *.cc)
BIN = $(SRC:%.cc=build/%)
OS := $(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS += -x objective-c++ -fobjc-arc
  LDFLAGS += -framework Foundation
endif

test: all
	${BIN} --expected_arch=${OS}

all: build ${BIN}

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

check: build build/CHECK_OK

build/CHECK_OK: build *.h */*.h ../*/*.h
	for i in *.h ; do \
		echo -n $(basename $$i)': ' ; \
		ln -sf ${PWD}/$$i ${PWD}/build/$$i.cc ; \
		if [ ! -f build/$$i.h.o -o build/$$i.h.cc -nt build/$$i.h.o ] ; then \
			${CPP} -I . ${CPPFLAGS} -c build/$$i.cc -o build/$$i.h.o ${LDFLAGS} || exit 1 ; echo 'OK' ; \
		else \
			echo 'Already OK' ; \
		fi \
	done && echo OK >$@

clean:
	rm -rf build

build:
	mkdir -p $@

build/%: %.cc *.h */*.h ../*/*.h
	${CPP} ${CPPFLAGS} -o $@ $< ${LDFLAGS}
