include KnowSheet/scripts/Makefile

.PHONY: all demo unittest

all: demo unittest

demo:
	(cd demo; make)

unittest:
	(cd test; make)
