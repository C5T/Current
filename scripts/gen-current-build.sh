#!/bin/bash

# Generates the `current_build.h` header.

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")

OS_VERSION="$(uname -a)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

EPOCH_SECONDS="$(date +'%s')"
if [[ $? -ne 0 ]]; then
	exit 1
fi

COMPILER_INFO="$($CPLUSPLUS --version 2>&1)"
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

GIT_DIFF_NAMES_MULTILINE="$(git diff --name-only)"
GIT_DIFF_NAMES=${GIT_DIFF_NAMES_MULTILINE//$'\n'/\\n}

GIT_DIFF_MD5SUM="$(git diff --no-ext-diff | md5sum)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

cat >$1 << EOF
// clang-format off

#ifndef CURRENT_BUILD_H
#define CURRENT_BUILD_H

#include "$SCRIPT_DIR/../port.h"

#include <chrono>
#include <vector>
#include <string>

#include "$SCRIPT_DIR/../TypeSystem/struct.h"
#include "$SCRIPT_DIR/../Bricks/strings/split.h"

namespace current {
namespace build {

constexpr static const char* kGitCommit = "$GIT_COMMIT";
constexpr static const char* kGitBranch = "$GIT_BRANCH";
constexpr static const char* kOS = "$OS_VERSION";
constexpr static const char* kCompiler = "$CPLUSPLUS";
constexpr static const char* kCompilerFlags = "$CPPFLAGS";
constexpr static const char* kLinkerFlags = "$LDFLAGS";
constexpr static const char* kCompilerInfo = "$COMPILER_INFO";

inline const std::vector<std::string>& GitDiffNames() {
  static std::vector<std::string> result = current::strings::Split("$GIT_DIFF_NAMES", '\n');
  return result;
}

CURRENT_STRUCT(Info) {
  CURRENT_FIELD(build_time, std::string, __DATE__ ", " __TIME__);
  CURRENT_FIELD(build_dir, std::string, "$PWD");
  CURRENT_FIELD(build_user, std::string, "$(whoami)");
  CURRENT_FIELD(build_time_epoch_microseconds, std::chrono::microseconds, std::chrono::microseconds(1000ull * 1000ull * $EPOCH_SECONDS));
  CURRENT_FIELD(git_commit_hash, std::string, kGitCommit);
  CURRENT_FIELD(git_dirty_files, (std::vector<std::string>), GitDiffNames());
  CURRENT_FIELD(git_branch, std::string, kGitBranch);
  CURRENT_FIELD(os, std::string, kOS);
  CURRENT_FIELD(compiler, std::string, kCompiler);
  CURRENT_FIELD(compiler_flags, std::string, kCompilerFlags);
  CURRENT_FIELD(linker_flags, std::string, kLinkerFlags);
  CURRENT_FIELD(compiler_info, std::string, kCompilerInfo);
  std::tuple<
    std::string,
    std::string,
    std::string,
    std::chrono::microseconds,
    std::string,
    std::vector<std::string>,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string>
  AsTuple() const {
    return std::tie(
      build_time,
      build_dir,
      build_user,
      build_time_epoch_microseconds,
      git_commit_hash,
      git_dirty_files,
      git_branch,
      os,
      compiler,
      compiler_flags,
      linker_flags,
      compiler_info);
  }
  bool operator==(const Info& rhs) const {
    return AsTuple() == rhs.AsTuple();
  }
  bool operator!=(const Info& rhs) const {
    return !operator==(rhs);
  }
};

}  // namespace current::build
}  // namespace current

#endif  // CURRENT_BUILD_H

// clang-format on

// Not to make it to the structure above, but for the human reader to be able to look into later.

/*
$ git diff --name_only
$GIT_DIFF_NAMES_MULTILINE

$ git status
$GIT_STATUS

$ git diff --no-ext-diff | md5sum
$GIT_DIFF_MD5SUM
*/
EOF
