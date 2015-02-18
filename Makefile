# TODO(dkorolev): Revisit the `test` and `all` targets here and in subdirectories.

.PHONY: default docu README.md test all clean check indent wc

default: test docu

test:
	./KnowSheet/scripts/full-test.sh

docu: README.md

README.md:
	./KnowSheet/scripts/gen-docu.sh >$@

all:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make) ; done

clean:
	rm -rf .noshit
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make clean) ; done

check:
	./KnowSheet/scripts/check-all-headers.sh

indent:
	./KnowSheet/scripts/indent.sh

wc:
	(for i in "*.cc" "*.h" "*.mm" "*.sh" Makefile; do find . -iname "$$i" -type f; done) \
	| grep -v "/.noshit/" | grep -v "/3party/" | xargs wc -l | sort -g
