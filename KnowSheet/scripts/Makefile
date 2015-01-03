# A universal Makefile for C++ modules of KnowSheet.
#
# Compiles and runs test.cc as the default target called `test`.
#
# Also supports `all` (build *.cc), `clean`, `indent` (via clang-format), `check` and `coverage`.

# TODO(dkorolev): Add a top-level 'make update' target to update KnowSheet from GitHub.

.PHONY: test all indent clean check coverage

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -g -Wall -W
LDFLAGS=-pthread

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=.noshit/%)

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS+= -x objective-c++ -fobjc-arc
  LDFLAGS+= -framework Foundation
endif

test: all
	.noshit/test --bricks_runtime_arch=${OS}

all: .noshit ${BIN}

clean:
	rm -rf .noshit impl/.noshit

.noshit:
	mkdir -p $@

.noshit/%: %.cc *.h
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

indent:
	../KnowSheet/scripts/indent.sh

check:
	../KnowSheet/scripts/check-headers.sh

coverage:
	../KnowSheet/scripts/coverage-report.sh
