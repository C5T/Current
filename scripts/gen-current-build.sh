#!/bin/bash

# Generates the `current_build.h` header.

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")

OS_VERSION="$(uname -a)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

COMPILER_INFO="$(${CPLUSPLUS:-g++} -v 2>&1)"
if [[ $? -ne 0 ]]; then
	exit 1
fi
COMPILER_INFO=${COMPILER_INFO//$'\n'/\\n}  # JSON-friendly newlines.

GIT_COMMIT="$(git rev-parse HEAD 2>/dev/null || echo '<not under git>')"
GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '<not under git>')"
GIT_STATUS="$(git status 2>/dev/null || echo '<not under git>')"

# NOTE(dkorolev): Removing the commands that may potentially run for a long time.
#GIT_DIFF_NAMES_MULTILINE="$(git diff --name-only 2>/dev/null || echo '<not under git>')"
#GIT_DIFF_NAMES=${GIT_DIFF_NAMES_MULTILINE//$'\n'/\\n}
#if [[ "$OSTYPE" == "darwin"* ]]; then
#  MD5SUM=md5
#else
#  MD5SUM=md5sum
#fi
#GIT_DIFF_MD5SUM="$(git diff --no-ext-diff 2>/dev/null | $MD5SUM || echo '<not under git>')"

OUTPUT_FILE="${1:-/dev/stdout}"

if [ "$1" != "" ] ; then
  mkdir -p "$(dirname "$1")" >/dev/null 2>&1
fi

cat >"$OUTPUT_FILE" << EOF
// NOTE: With 'C5T_CMAKE_PROJECT' '#define'-d it takes ~0.03 seconds to "build" this file.

// clang-format off

#ifndef CURRENT_BUILD_H
#define CURRENT_BUILD_H

#ifndef C5T_CMAKE_PROJECT
#include "$SCRIPT_DIR/../port.h"
#include <chrono>
#include <ctime>
#include <vector>
#include <string>
#include "$SCRIPT_DIR/../typesystem/struct.h"
#include "$SCRIPT_DIR/../typesystem/optional.h"
#include "$SCRIPT_DIR/../bricks/strings/split.h"
#endif  // C5T_CMAKE_PROJECT

namespace current {
namespace build {

#ifdef C5T_CMAKE_PROJECT
namespace cmake {
#endif  // C5T_CMAKE_PROJECT

constexpr static unsigned long kCurrentBuildHeaderUnixEpochSeconds = $(date +%s);

constexpr static const char* kBuildDateTime = __DATE__ ", " __TIME__;
constexpr static const char* kGitCommit = "$GIT_COMMIT";
constexpr static const char* kGitBranch = "$GIT_BRANCH";
constexpr static const char* kOS = "$OS_VERSION";
constexpr static const char* kCompiler = "${CPLUSPLUS:-default[g++]}";
constexpr static const char* kCompilerFlags = "$CPPFLAGS";
constexpr static const char* kLinkerFlags = "$LDFLAGS";
constexpr static const char* kCompilerInfo = "$COMPILER_INFO";

#ifndef C5T_CMAKE_PROJECT
inline const std::vector<std::string>& GitDiffNames() {
  static std::vector<std::string> result = current::strings::Split("$GIT_DIFF_NAMES", '\n');
  return result;
}

// TODO(dkorolev): Maybe this function should be elsewhere, under some "proper" Current source dir?

// A hacky, but cross-platform way to parse \`kBuildDateTime\` since:
// * \`__DATE__\` always contains English months,
// * GCC < 5.0 doesn't have \`get_time\`,
// * \`strptime\` is locale-dependent and \`setlocale\` is not thread-safe.
inline std::chrono::microseconds BuildTimestamp() {
  CURRENT_ASSERT(strlen(kBuildDateTime) == 21u); // "mmm dd yyyy, hh:mm:ss".
  const char* s = kBuildDateTime;
  std::tm tm;
  // LCOV_EXCL_START
  if (s[0] == 'J') {
    if (s[1] == 'a') {
      tm.tm_mon = 0;  // January.
    } else if (s[2] == 'n') {
      tm.tm_mon = 5;  // June.
    } else {
      tm.tm_mon = 6;  // July.
    }
  } else if (s[0] == 'F') {
    tm.tm_mon = 1;  // February.
  } else if (s[0] == 'M') {
    if (s[2] == 'r') {
      tm.tm_mon = 2;  // March.
    } else {
      tm.tm_mon = 4;  // May.
    }
  } else if (s[0] == 'A') {
    if (s[1] == 'p') {
      tm.tm_mon = 3;  // April.
    } else {
      tm.tm_mon = 7;  // August.
    }
  } else if (s[0] == 'S') {
      tm.tm_mon = 8;  // September.
  } else if (s[0] == 'O') {
      tm.tm_mon = 9;  // October.
  } else if (s[0] == 'N') {
      tm.tm_mon = 10;  // November.
  } else {
      tm.tm_mon = 11;  // December.
  }
  // LCOV_EXCL_STOP
  tm.tm_mday = (s[4] >= '0' ? (s[4] - '0') * 10 : 0) + (s[5] - '0');
  tm.tm_year = ((s[7] - '0') * 1000 + (s[8] - '0') * 100 + (s[9] - '0') * 10 + (s[10] - '0')) - 1900;
  tm.tm_hour = (s[13] - '0') * 10 + (s[14] - '0');
  tm.tm_min = (s[16] - '0') * 10 + (s[17] - '0');
  tm.tm_sec = (s[19] - '0') * 10 + (s[20] - '0');
  tm.tm_isdst = -1;
  time_t tt = mktime(&tm);
  return std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::from_time_t(tt)).time_since_epoch();
}

CURRENT_STRUCT(BuildInfo) {
  CURRENT_FIELD(build_time, std::string, kBuildDateTime);
  CURRENT_FIELD_DESCRIPTION(build_time, "The date and time of the build, in 'mmm dd yyyy, hh:mm:ss' format.");

  CURRENT_FIELD(build_dir, std::string, "$PWD");
  CURRENT_FIELD_DESCRIPTION(build_dir, "The working directory at the moment of building the binary.");

  CURRENT_FIELD(build_user, std::string, "$(whoami)");
  CURRENT_FIELD_DESCRIPTION(build_user, "The system ID of the user who built the binary (\`whoami\`).");

  CURRENT_FIELD(build_time_epoch_microseconds, std::chrono::microseconds, BuildTimestamp());
  CURRENT_FIELD_DESCRIPTION(build_time_epoch_microseconds, "Unix epoch microseconds of when the binary was built.");

  CURRENT_FIELD(os, std::string, kOS);
  CURRENT_FIELD_DESCRIPTION(os, "The information about the operating system.");

  CURRENT_FIELD(git_commit_hash, Optional<std::string>, std::string(kGitCommit));
  CURRENT_FIELD_DESCRIPTION(git_commit_hash, "The hash of the Git commit used for building the binary.");

  CURRENT_FIELD(git_dirty_files, Optional<std::vector<std::string>>, GitDiffNames());
  CURRENT_FIELD_DESCRIPTION(git_dirty_files, "The list of the Git dirty files.");

  CURRENT_FIELD(git_branch, Optional<std::string>, std::string(kGitBranch));
  CURRENT_FIELD_DESCRIPTION(git_branch, "The name of the Git branch used for building the binary.");

  CURRENT_FIELD(compiler, Optional<std::string>, std::string(kCompiler));
  CURRENT_FIELD_DESCRIPTION(compiler, "The command used to invoke the compiler.");

  CURRENT_FIELD(compiler_flags, Optional<std::string>, std::string(kCompilerFlags));
  CURRENT_FIELD_DESCRIPTION(compiler_flags, "The flags passed to the compiler.");

  CURRENT_FIELD(linker_flags, Optional<std::string>, std::string(kLinkerFlags));
  CURRENT_FIELD_DESCRIPTION(linker_flags, "The flags passed to the linker.");

  CURRENT_FIELD(compiler_info, Optional<std::string>, std::string(kCompilerInfo));
  CURRENT_FIELD_DESCRIPTION(compiler_info, "The output of the \`\$CPLUSPLUS -v\` command.");

  std::tuple<
    std::string,
    std::string,
    std::string,
    std::chrono::microseconds,
    std::string,
    Optional<std::string>,
    Optional<std::vector<std::string>>,
    Optional<std::string>,
    Optional<std::string>,
    Optional<std::string>,
    Optional<std::string>,
    Optional<std::string>>
  AsTuple() const {
    return std::tie(
      build_time,
      build_dir,
      build_user,
      build_time_epoch_microseconds,
      os,
      git_commit_hash,
      git_dirty_files,
      git_branch,
      compiler,
      compiler_flags,
      linker_flags,
      compiler_info);
  }
  bool operator==(const BuildInfo& rhs) const {
    return AsTuple() == rhs.AsTuple();
  }
  bool operator!=(const BuildInfo& rhs) const {
    return !operator==(rhs);
  }
};
#endif  // C5T_CMAKE_PROJECT

#ifdef C5T_CMAKE_PROJECT
}  // namespace current::build::cmake
using namespace cmake;
#endif  // C5T_CMAKE_PROJECT

}  // namespace current::build
}  // namespace current

#endif  // CURRENT_BUILD_H

// clang-format on
EOF

# NOTE(dkorolev): The commands below can be slow to run, and they're best to remove now, at least from `C5T/stable`.
cat >/dev/null <<EOF
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
