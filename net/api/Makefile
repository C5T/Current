.PHONY: test all indent check clean coverage

CPLUSPLUS ?= g++
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

check:
	../../scripts/check-headers.sh

clean:
	rm -rf build

build:
	mkdir -p $@

build/%: %.cc *.h */*.h ../*/*.h
	${CPLUSPLUS} ${CPPFLAGS} -o $@ $< ${LDFLAGS}

coverage:
	../../scripts/coverage-report.sh
