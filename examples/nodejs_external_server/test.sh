#!/bin/bash

echo '[ test      ] Starting.'
(sleep 1.1; curl -d 'Test request to server one.' 'localhost:3001/one') &
(sleep 1.4; curl -d 'Test request to server two.' 'localhost:3002/two') &
(sleep 1.7; curl -X DELETE 'localhost:3001/one') &
(sleep 1.9; curl -X DELETE 'localhost:3002/two') &
./run.js
echo '[ test      ] Waiting for the scheduled `curl`-s to complete.'
wait
echo '[ test      ] The scheduled `curl`-s are completed, and the test is done.'
echo '[ test      ] Ignore the possibly garbled output above, this test has no thread-safe stdout synchronization.'
echo '[ test      ] The test has succeeded if `One: responding to "Test request to server one.".` and'
echo '[ test      ]                           `Two: responding to "Test request to server two.".` are present above.'
