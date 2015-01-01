.PHONY: test all indent clean check coverage

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -g -Wall -W
LDFLAGS=-pthread

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=.noshit/%)

test: all
	.noshit/test

all: .noshit ${BIN}

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	rm -rf .noshit

check:
	../KnowSheet/scripts/check-headers.sh

.noshit:
	mkdir -p $@

.noshit/%: %.cc *.h
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

coverage:
	../KnowSheet/scripts/coverage-report.sh
