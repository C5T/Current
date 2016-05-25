# In `Makefile`, do nothing for `current_build.h`. Use `MakefileWithCurrentBuild` to have it created.

.PHONY: phony_current_build

SCRIPTS_DIR := $(shell dirname "$(shell readlink "${PWD}/Makefile")")
include ${SCRIPTS_DIR}/MakefileImpl

phony_current_build:
