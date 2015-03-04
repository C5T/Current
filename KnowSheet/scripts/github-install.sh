#!/bin/bash
#
# This script downloads the master branch of some GitHub repository and installs it in current directory.

set -u -e

# Command-line parameters.
GITHUB_REPO="${1:-Bricks}"
GITHUB_USER="${2:-KnowSheet}"
GITHUB_BRANCH="${3:-master}"

GITHUB_REPO_DASH_BRANCH="${GITHUB_REPO}-${GITHUB_BRANCH}"

# The URL to fetch a .tar.gz from.
URL="https://github.com/${GITHUB_USER}/${GITHUB_REPO}/archive/${GITHUB_BRANCH}.tar.gz"

# A temporary directory, expected to be .gitignore-d.
TMP_DIR_NAME=".noshit"

(
  mkdir -p "$TMP_DIR_NAME"
  cd "$TMP_DIR_NAME"
  rm -rf "$GITHUB_REPO" "$GITHUB_REPO_DASH_BRANCH"
  echo -n "github.com/${GITHUB_USER}/${GITHUB_REPO}@${GITHUB_BRANCH}: Fetch... "
  curl -s -L "$URL" | tar xfz - || (echo "Failed, please check whether $URL can be fetched."; exit 1)
  mv "$GITHUB_REPO_DASH_BRANCH" "$GITHUB_REPO"
  echo -ne "\b\b\b\b\b\b\b\b\b"
)

if [ -d "$GITHUB_REPO" ] ; then
  if diff -r "$GITHUB_REPO" "$TMP_DIR_NAME/$GITHUB_REPO" ; then
    echo "Up to date."
  else
    echo "Conflict, please fix or reinstall."
    echo "For a quick workaround, try renaming the current revision under another name:"
    echo "mv \"$GITHUB_REPO\" \"$TMP_DIR_NAME/${GITHUB_REPO}-$(date +%Y%m%d-%H%M%S)\""
    echo "and then re-run the same command."
    exit 1
  fi
else
  mv "$TMP_DIR_NAME/$GITHUB_REPO" .
  echo "Installed."
fi
