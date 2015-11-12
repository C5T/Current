# A universal Makefile for Current C++ code.
#
# By default, runs the test (compiled from `test.cc`) if present, or just runs all the binaries one after another.
#
# Also supports `all` (build *.cc), `clean`, `indent` (via clang-format), `check` and `coverage`.

# TODO(dkorolev): Add a top-level 'make update' target to update from GitHub.

.PHONY: test all indent clean check coverage readlink

# Need to know where to invoke scripts from, since `Makefile` can be a relative path symlink.
MAKEFILE_NAME = $(lastword $(MAKEFILE_LIST))
CURRENT_SCRIPTS_DIR := $(patsubst %\,%,$(patsubst %/,%,$(dir $(shell readlink "$(MAKEFILE_NAME)" || echo "$(MAKEFILE_NAME)"))))
CURRENT_SCRIPTS_DIR_FULL_PATH := $(shell "$(CURRENT_SCRIPTS_DIR)/fullpath.sh" "$(CURRENT_SCRIPTS_DIR)" )

CPLUSPLUS?=g++
CPPFLAGS=-std=c++11 -Wall -W -ftemplate-depth=4096
ifeq ($(NDEBUG),1)
CPPFLAGS+= -O3
else
# Commented out by @dkorolev -- this flag makes compilation slower for no good reason.
# CPPFLAGS+= -g
endif
LDFLAGS=-pthread

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=.current/%)

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS+= -stdlib=libc++ -x objective-c++ -fobjc-arc
  LDFLAGS+= -framework Foundation
endif

default: all
	if [ -f test.cc ] ; then \
		make test ;\
	else \
		find .current/ -perm +111 -type f -exec "{}" ";" ; \
	fi

test: .current/test
	.current/test --bricks_runtime_arch=${OS}

debug:
	ulimit -c unlimited && touch test.cc && rm -f core && make ./.current/test && (./.current/test && echo OK || gdb ./.current/test core)

all: ${BIN}

clean:
	rm -rf .current core impl/.current impl/core

.current/%: %.cc *.h
	mkdir -p .current
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

indent:
	${CURRENT_SCRIPTS_DIR_FULL_PATH}/indent.sh

check:
	${CURRENT_SCRIPTS_DIR_FULL_PATH}/check-headers.sh

coverage:
	${CURRENT_SCRIPTS_DIR_FULL_PATH}/coverage-report.sh

# For Linux vs. Mac debugging purposes.
readlink:
	echo ${CURRENT_SCRIPTS_DIR}

wc:
	echo -n "Total files: " ; (find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | wc -l
	(find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | xargs wc -l | sort -gr
