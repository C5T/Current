#!/usr/bin/env bash

N="$(ls *.cc | wc -l)"
if [[ "$N" -ne "1" ]] ; then
  echo -e "\033[31m$N\033[0m" '== `COUNT(*.cc)`'"\033[0m != 1"
else
  $(dirname "${BASH_SOURCE[0]}")/gen_vcxproj_body.sh >"$(basename "$PWD").vcxproj"
  echo -e "\033[32m\033[1mOK\033[0m"
fi
