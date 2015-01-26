# A universal Makefile for C++ modules of KnowSheet.
#
# By default, runs the test (compiled from `test.cc`) if present, or just runs all the binaries one after another.
#
# Also supports `all` (build *.cc), `clean`, `indent` (via clang-format), `check` and `coverage`.

# TODO(dkorolev): Add a top-level 'make update' target to update KnowSheet from GitHub.

.PHONY: test all indent clean check coverage

# Need to know where to invoke scripts from, since `Makefile` can be a relative path symlink.
KNOWSHEET_SCRIPTS_DIR=$(dir $(shell readlink -e $(lastword $(MAKEFILE_LIST))))

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -Wall -W
ifeq ($(NDEBUG),1)
CPPFLAGS+= -O3
else
CPPFLAGS+= -g
endif
LDFLAGS=-pthread

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=.noshit/%)

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS+= -x objective-c++ -fobjc-arc
  LDFLAGS+= -framework Foundation
endif

default: all
	if [ -f test.cc ] ; then \
		.noshit/test --bricks_runtime_arch=${OS} ; \
	else \
		find .noshit/ -executable -type f -exec "{}" ";" ; \
	fi

debug:
	ulimit -c unlimited && touch test.cc && rm -f core && make ./.noshit/test && (./.noshit/test && echo OK || gdb ./.noshit/test core)

all: ${BIN}

clean:
	rm -rf .noshit core impl/.noshit impl/core

.noshit/%: %.cc *.h
	mkdir -p .noshit
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

indent:
	${KNOWSHEET_SCRIPTS_DIR}/indent.sh

check:
	${KNOWSHEET_SCRIPTS_DIR}/check-headers.sh

coverage:
	${KNOWSHEET_SCRIPTS_DIR}/coverage-report.sh
