#!/bin/bash
(make .current/karl_server && npm i && ./node_modules/mocha/bin/mocha test)
