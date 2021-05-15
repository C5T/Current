#!/bin/bash
#
# This script is what `make check` does in Current: it confirms that each `.hpp` header can be "built" by itself.

DIR=$(mktemp -d)

echo "Dir: $DIR"

cat <<EOF >$DIR/package.json
{
  "name": "current-nodejs-integration-test",
  "private": true,
  "version": "1.0.0",
  "dependencies": {
    "node-addon-api": "^1.7.2"
  },
  "gypfile": true,
  "keywords": [
    "current"
  ],
  "author": "Dima Korolev",
  "license": "MIT"
}
EOF

(cd $DIR; npm i) || exit 1

cat <<EOF >$DIR/binding.gyp
{
  'targets': [
    {
      'target_name': 'current-nodejs-check-sh',
      'sources': [ 'test.cpp' ],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")", "$PWD/../../integrations/nodejs"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }
  ]
}
EOF

touch $DIR/test.cpp
(cd $DIR; node-gyp configure) || exit 1

for HEADER in *.hpp ; do
  echo
  echo 'Checking '$HEADER' ...'
  echo "#include \"$HEADER\"" >$DIR/test.cpp
  (cd $DIR; node-gyp build) || exit 1
  echo 'Checking '$HEADER' : OK.'
done

echo
echo 'Checking all headers: OK.'
