#!/bin/bash

# This script demonstrates how does the standalone EJS node.js-powered binary behaves.
#
# To run the service -- for instance, for the C++ demo -- use `npm run start`.

PREREQUISITES="""
  sudo apt install npm
  sudo npm i -g n
  sudo n stable
  npm i
"""

set -u -e

if ! npm i ; then
  echo 'Prerequisite: `npm`.'
  exit 1
fi

TMPFILE=$(mktemp)
node server.js > $TMPFILE &
SERVER_PID=$!

while ! grep -q listening $TMPFILE ; do sleep 0.01 ; done

curl -H "Content-Type: application/json" -d '{"a":1,"b":10,"x":[2,3,5,7]}' localhost:3001

kill $SERVER_PID
rm $TMPFILE
