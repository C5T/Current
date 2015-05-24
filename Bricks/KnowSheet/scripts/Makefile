# A universal Makefile for C++ modules of KnowSheet.
#
# By default, runs the test (compiled from `test.cc`) if present, or just runs all the binaries one after another.
#
# Also supports `all` (build *.cc), `clean`, `indent` (via clang-format), `check` and `coverage`.

# TODO(dkorolev): Add a top-level 'make update' target to update KnowSheet from GitHub.

.PHONY: test all indent clean check coverage readlink

# Need to know where to invoke scripts from, since `Makefile` can be a relative path symlink.
KNOWSHEET_SCRIPTS_DIR := $(patsubst %\,%,$(patsubst %/,%,$(dir $(shell readlink $(lastword $(MAKEFILE_LIST))))))
KNOWSHEET_SCRIPTS_DIR_FULL_PATH := $(shell "$(KNOWSHEET_SCRIPTS_DIR)/KnowSheetReadlink.sh" "$(KNOWSHEET_SCRIPTS_DIR)" )

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -Wall -W
ifeq ($(NDEBUG),1)
CPPFLAGS+= -O3
else
# Commented out by @dkorolev -- this flag makes compilation slower for no good reason.
# CPPFLAGS+= -g
endif
LDFLAGS=-pthread

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=.noshit/%)

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS+= -stdlib=libc++ -x objective-c++ -fobjc-arc
  LDFLAGS+= -framework Foundation
endif

default: all
	if [ -f test.cc ] ; then \
		make test ;\
	else \
		find .noshit/ -executable -type f -exec "{}" ";" ; \
	fi

test: .noshit/test
	.noshit/test --bricks_runtime_arch=${OS}

debug:
	ulimit -c unlimited && touch test.cc && rm -f core && make ./.noshit/test && (./.noshit/test && echo OK || gdb ./.noshit/test core)

all: ${BIN}

clean:
	rm -rf .noshit core impl/.noshit impl/core

.noshit/%: %.cc *.h
	mkdir -p .noshit
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

indent:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/indent.sh

check:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/check-headers.sh

coverage:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/coverage-report.sh

# For Linux vs. Mac debugging purposes.
readlink:
	echo ${KNOWSHEET_SCRIPTS_DIR}
