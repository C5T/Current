.PHONY: test full_test full_debug_test clean

test:
	npm i && npm test

full_test:  # NOTE(dkorolev): These extra commands are unnecessary, just keeping them here for the reference.
	npm install && node-gyp configure && node-gyp build && npm test

full_debug_test:
	npm install && node-gyp configure --debug && node-gyp build --debug && npm test

clean:
	rm -rf build node_modules core
