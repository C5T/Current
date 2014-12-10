.PHONY: test all indent clean

CPP = clang++
CPPFLAGS = -std=c++11 -Wall
LDFLAGS = -pthread

SRC=$(wildcard *.cc)
BIN = $(SRC:%.cc=build/%)
OS := $(shell uname)
ifeq ($(OS),Darwin)
  CPPFLAGS += -x objective-c++ -fobjc-arc
  LDFLAGS += -framework Foundation
endif

test: all
	${BIN}

all: build ${BIN}

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	rm -rf build

build:
	mkdir -p $@

build/%: %.cc *.h
	${CPP} ${CPPFLAGS} -o $@ $< ${LDFLAGS}
