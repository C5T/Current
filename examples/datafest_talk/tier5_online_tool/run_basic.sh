#!/bin/bash
#
# Uploads the script into the running service, and, if successful, runs it.

if [[ $# -ne 1 ]] ; then
  echo "Need one argument: A .cpp file to 'run'."
  exit -1
fi

CPP=$1
URL=http://localhost:3000

TMPFILE=$(mktemp)
if ! curl -s -H "X-Source-Name: $CPP" --data-binary @$CPP $URL > $TMPFILE ; then
  echo "Cannot PUT."
  exit 1
fi

if grep "Compilation error:" $TMPFILE > /dev/null ; then
  cat $TMPFILE
  exit 1
else
  ID=$(cut -f2 -d@ $TMPFILE)
  echo "To run with extra verbose output: curl -i $URL/$ID" > /dev/stderr
  echo > /dev/stderr
  curl -s $URL/$ID | tee >(md5sum)
fi
