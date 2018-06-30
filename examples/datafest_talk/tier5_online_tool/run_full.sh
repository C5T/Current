#!/bin/bash
#
# Uploads the script into the running service, and, if successful, starts a browser to execute it.

if [[ $# -ne 1 ]] ; then
  echo "Need one argument: A .cpp file to 'run'."
  exit -1
fi

CPP=$1
URL=http://localhost:3000/full

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
  echo "Opening $URL/$ID".
  if [ "$(uname)" = "Linux" ] ; then
    sensible-browser $URL/$ID >/dev/null 2>&1
  else
    open $URL/$ID
  fi
  echo "Done."
fi
