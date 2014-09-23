# TODO(dkorolev): Add more make targets here.

.PHONY: indent

indent:
	find . -regextype posix-egrep -regex ".*\.(cc|h)" | xargs clang-format-3.5 -i
