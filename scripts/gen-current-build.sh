#!/bin/bash

# Generates the `current_build.h` header. Only updates the file if it has been changed.

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")

OS_VERSION="$(uname -a)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

COMPILER_INFO="$($CPLUSPLUS -v 2>&1)"
if [[ $? -ne 0 ]]; then
	exit 1
fi
COMPILER_INFO=${COMPILER_INFO//$'\n'/\\n}  # JSON-friendly newlines.

GIT_COMMIT="$(git rev-parse HEAD)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

GIT_STATUS="$(git status)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

GIT_DIFF_MD5SUM="$(git diff --no-ext-diff | md5sum)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

cat >$1.tmp << EOF
#ifndef CURRENT_BUILD_H
#define CURRENT_BUILD_H

#include "$SCRIPT_DIR/../port.h"

#include <string>

#include "$SCRIPT_DIR/../TypeSystem/struct.h"

namespace current {
namespace build {

constexpr static const char* kGitCommit = "$GIT_COMMIT";
constexpr static const char* kGitBranch = "$GIT_BRANCH";
constexpr static const char* kOS = "$OS_VERSION";
constexpr static const char* kCompiler = "$CPLUSPLUS";
constexpr static const char* kCompilerFlags = "$CPPFLAGS";
constexpr static const char* kLinkerFlags = "$LDFLAGS";
constexpr static const char* kCompilerInfo = "$COMPILER_INFO";

CURRENT_STRUCT(Info) {
  CURRENT_FIELD(date_time, std::string, __DATE__ ", " __TIME__);
  CURRENT_FIELD(git_commit, std::string, kGitCommit);
  CURRENT_FIELD(git_branch, std::string, kGitBranch);
  CURRENT_FIELD(os, std::string, kOS);
  CURRENT_FIELD(compiler, std::string, kCompiler);
  CURRENT_FIELD(compiler_flags, std::string, kCompilerFlags);
  CURRENT_FIELD(linker_flags, std::string, kLinkerFlags);
  CURRENT_FIELD(compiler_info, std::string, kCompilerInfo);
};

}  // namespace current::build
}  // namespace current

#endif  // CURRENT_BUILD_H

/*
$ # Extra pieces of data to force \`make\` rebuild the code as repository state changes.

$ git status
$GIT_STATUS

$ git diff --no-ext-diff | md5sum
$GIT_DIFF_MD5SUM
*/
EOF

if ! [ -f $1 ] ; then
  echo "scripts/gen-current-build.sh > $1"  # New.
  mv $1.tmp $1
elif ! diff -q $1.tmp $1 > /dev/null 2>&1 ; then
  # Updated file; overwrite.
  echo "rm $1 && scripts/gen-current-build.sh > $1"  # Overwrite.
  mv -f $1.tmp $1
else
  rm $1.tmp  # Ignore.
fi
