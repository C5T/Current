.PHONY: test all indent clean check coverage

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -g -Wall -W
LDFLAGS=-pthread
CPPFLAGS_FOR_COVERAGE=${CPPFLAGS} -O0 -g -fprofile-arcs -ftest-coverage
LDFLAGS_FOR_COVERAGE=${LDFLAGS}

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=build/%)
BIN_FOR_COVERAGE=$(SRC:%.cc=build/coverage/%)

test: all
	./build/test

all: build ${BIN}

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	rm -rf build .tmp

check:
	../scripts/check-headers.sh

build:
	mkdir -p $@

build/%: %.cc *.h
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

coverage:
	../scripts/coverage-report.sh
