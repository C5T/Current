.PHONY: all

all:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd $$i; make) ; done
