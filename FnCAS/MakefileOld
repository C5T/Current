include ../scripts/Makefile
LDFLAGS+=-ldl

.PHONY: install docu README.md deprecated_test

install:
	../scripts/github-install.sh

docu: README.md

README.md:
	../scripts/gen-docu.sh >$@

deprecated_test:
	(cd deprecated_test; make)
