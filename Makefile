# TODO(dkorolev): Revisit the `test` and `all` targets here and in subdirectories.

.PHONY: test all clean check indent wc

test:
	./KnowSheet/scripts/full-test.sh

all:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make) ; done

clean:
	rm -rf .noshit
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make clean) ; done

check:
	./KnowSheet/scripts/check-all-headers.sh

indent:
	(find . -name "*.cc" ; find . -name "*.h") | grep -v "/.noshit/" | grep -v 3party | xargs clang-format-3.5 -i

wc:
	(find . -iname "*.cc" ; find . -iname "*.h") | grep -v "/.noshit/" | grep -v 3party | xargs wc -l
