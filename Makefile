include KnowSheet/scripts/Makefile
LDFLAGS+=-ldl

.PHONY: install deprecated_test

install:
	./KnowSheet/scripts/github-install.sh

deprecated_test:
	(cd deprecated_test; make)
