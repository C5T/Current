#!/usr/bin/env bash

$(dirname "${BASH_SOURCE[0]}")/gen_vcxproj_body.sh >"$(basename "$PWD").vcxproj"
