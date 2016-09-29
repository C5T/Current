#!/bin/bash
(make .current/karl_server &&
	../../../scripts/gen-current-build-json.sh current_build.json &&
	npm i &&
	./node_modules/mocha/bin/mocha test)
