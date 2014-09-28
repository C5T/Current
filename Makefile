# TODO(dkorolev): Add more make targets here.

.PHONY: all demo test indent

all: demo test

demo:
	(cd demo; make)

test:
	(cd test; make)

indent:
	find . -regextype posix-egrep -regex ".*\.(cc|h)" | xargs clang-format-3.5 -i
