.PHONY: all clean indent

all: build/js_external.a

clean:
	rm -rf build

indent:
	clang-format-10 -i *.cc *.h

build/js_external.a: js_external.cc js_external.h
	mkdir -p build
	g++ -std=c++17 -o build/js_external.a -c $< -fPIC
