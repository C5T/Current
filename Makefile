# TODO(dkorolev): Revisit the `test` and `all` targets here and in subdirectories.

.PHONY: default docu README.md test all clean check indent wc

KNOWSHEET_SCRIPTS_DIR := ./KnowSheet/scripts
KNOWSHEET_SCRIPTS_DIR_FULL_PATH := $(shell "$(KNOWSHEET_SCRIPTS_DIR)/KnowSheetReadlink.sh" "$(KNOWSHEET_SCRIPTS_DIR)" )

default: test docu

test:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/full-test.sh

docu: README.md

README.md:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/gen-docu.sh >"$@"

all:
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd "$$i"; make) ; done

clean:
	rm -rf .noshit
	for i in `find . -mindepth 1 -maxdepth 1 -type d | grep -v ".git" | grep -v 3party` ; do (cd "$$i"; make clean) ; done

check:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/check-all-headers.sh

indent:
	${KNOWSHEET_SCRIPTS_DIR_FULL_PATH}/indent.sh

wc:
	(for i in "*.cc" "*.h" "*.mm" "*.sh" Makefile; do find . -iname "$$i" -type f; done) \
	| grep -v "/.noshit/" | grep -v "/3party/" | xargs wc -l | sort -g
