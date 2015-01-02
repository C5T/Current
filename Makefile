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
	./KnowSheet/scripts/indent.sh

wc:
	(find . -iname "*.cc" ; find . -iname "*.h") | grep -v "/.noshit/" | grep -v 3party | xargs wc -l
