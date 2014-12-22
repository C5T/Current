.PHONY: all clean check indent wc

all:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make) ; done

clean:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make clean) ; done

check:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make check) ; done

indent:
	(find . -name "*.cc" ; find . -name "*.h") | grep -v "/build/" | grep -v 3party | xargs clang-format-3.5 -i

wc:
	(find . -iname "*.cc" ; find . -iname "*.h") | grep -v "/build/" | grep -v 3party | xargs wc -l
