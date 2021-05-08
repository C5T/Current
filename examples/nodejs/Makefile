.PHONY: test debug_test indent clean

test:  # NOTE(dkorolev): The `node-gyp` commands are not optional on Ubuntu but required on CentOS.
	npm install && node-gyp configure && node-gyp build && npm test

debug_test:
	npm install && node-gyp configure --debug && node-gyp build --debug && npm test

indent:
	clang-format-10 -i code.cpp

clean:
	rm -rf build node_modules core
