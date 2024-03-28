# NOTE(dkorolev):
#
# Yes, I am well aware it is ugly to have a `Makefile` for a `cmake`-built project.
#
# However, there is quite some value.
# Please see the README of https://github.com/C5T/Current/tree/stable/cmake for more details.
#
# Besides, this `Makefiles` makes `:mak` in Vim work like a charm!

.PHONY: release debug release_dir debug_dir release_test debug_test test fmt clean

# Then this `Makefile` is used to run a `cmake`-based Current build,
# user-defined dependencies go here.
# TODO(dkorolev): Support more than one of them on the `CMakeLists.txt` level.
# TODO(dkorolev): Test that this works with `leveldb` too.
C5T_DEPS=""

# Set this var to anything non-empty to speed up builds that do not require `googletest`.
C5T_NO_GTEST=""

DEBUG_BUILD_DIR=$(shell echo "$${DEBUG_BUILD_DIR:-.current_debug}")
RELEASE_BUILD_DIR=$(shell echo "$${RELEASE_BUILD_DIR:-.current}")

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CORES=$(shell sysctl -n hw.physicalcpu)
else
  CORES=$(shell nproc)
endif

CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format}")

release: release_dir CMakeLists.txt
	@MAKEFLAGS=--no-print-directory cmake --build "${RELEASE_BUILD_DIR}" -j ${CORES}

release_dir: ${RELEASE_BUILD_DIR} .gitignore
	@grep "^${RELEASE_BUILD_DIR}/$$" .gitignore >/dev/null || echo "${RELEASE_BUILD_DIR}/" >>.gitignore

${RELEASE_BUILD_DIR}: CMakeLists.txt src
	@C5T_DEPS="${C5T_DEPS}" C5T_NO_GTEST="${C5T_NO_GTEST}" cmake -DCMAKE_BUILD_TYPE=Release -B "${RELEASE_BUILD_DIR}" .

test: release
	@(cd "${RELEASE_BUILD_DIR}"; make test)

debug: debug_dir CMakeLists.txt
	@MAKEFLAGS=--no-print-directory cmake --build "${DEBUG_BUILD_DIR}" -j ${CORES}

debug_dir: ${DEBUG_BUILD_DIR} .gitignore
	@grep "^${DEBUG_BUILD_DIR}/$$" .gitignore >/dev/null || echo "${DEBUG_BUILD_DIR}/" >>.gitignore

${DEBUG_BUILD_DIR}: CMakeLists.txt src
	@C5T_DEPS="${C5T_DEPS}" C5T_NO_GTEST="${C5T_NO_GTEST}" cmake -B "${DEBUG_BUILD_DIR}" .

debug_test: debug
	@(cd "${DEBUG_BUILD_DIR}"; make test)

test: release_test

fmt:
	${CLANG_FORMAT} -i src/*.cc src/*.h

CMakeLists.txt:
	@curl -s https://raw.githubusercontent.com/C5T/Current/stable/cmake/CMakeLists.txt >$@

.gitignore:
	@touch .gitignore

clean:
	rm -rf "${DEBUG_BUILD_DIR}" "${RELEASE_BUILD_DIR}"
