#!/bin/bash

# Generates the minimalistic `current_build.json` file without compiler-related fields.

BUILD_DATETIME=$(LC_ALL=en_EN date "+%b %d %Y, %H:%M:%S")
BUILD_EPOCH_MICROSECONDS="$(date "+%s")000000"

OS_VERSION="$(uname -a)"
if [[ $? -ne 0 ]]; then
	exit 1
fi

OPTIONAL_GIT_FIELDS=""
GIT_COMMIT="$(git rev-parse HEAD)"
if [[ $? -ne 0 ]]; then
	OPTIONAL_GIT_FIELDS=""
else
	OPTIONAL_GIT_FIELDS+="\"git_commit_hash\": \"$GIT_COMMIT\","$'\n'
fi

GIT_DIFF_NAMES="$(git diff --name-only | sed -e 's/\(.*\)/"\1"/' | paste -d, -s)"
if [[ ${PIPESTATUS[0]} -ne 0 || $? -ne 0 ]]; then
	OPTIONAL_GIT_FIELDS=""
else
	OPTIONAL_GIT_FIELDS+="  \"git_dirty_files\": [$GIT_DIFF_NAMES],"$'\n'
fi

GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ $? -ne 0 ]]; then
	OPTIONAL_GIT_FIELDS=""
else
	OPTIONAL_GIT_FIELDS+="  \"git_branch\": \"$GIT_BRANCH\","
fi

cat >$1 << EOF
{
  "build_time": "$BUILD_DATETIME",
  "build_dir": "$PWD",
  "build_user": "$(whoami)",
  "build_time_epoch_microseconds": $BUILD_EPOCH_MICROSECONDS,
  $OPTIONAL_GIT_FIELDS
  "os": "$OS_VERSION"
}
EOF
