.PHONY: test debug_test indent clean

test:  # NOTE(dkorolev): The `node-gyp` commands are not optional on Ubuntu but required on CentOS.
	(cd external; make) && npm install && node-gyp configure && node-gyp build && ./test.sh

debug_test:
	(cd external; make) && npm install && node-gyp configure --debug && node-gyp build --debug && ./test.sh

indent:
	(cd external; make indent) && clang-format-10 -i js_native.cpp

clean:
	(cd external; make clean)
	rm -rf build node_modules core
