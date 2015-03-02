include KnowSheet/scripts/Makefile
LDFLAGS+=-ldl

.PHONY: install docu README.md deprecated_test

install:
	./KnowSheet/scripts/github-install.sh

docu: README.md

README.md:
	./KnowSheet/scripts/gen-docu.sh >$@

deprecated_test:
	(cd deprecated_test; make)
