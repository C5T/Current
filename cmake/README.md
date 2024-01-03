# `C5T/current/cmake/`

This directory contains the `CMakeLists.txt`-based setup for a user project that needs Current.

This `CMakeLists.txt` is accompanied by a `Makefile`. Together, they:

* Make the user depend only on the `Makefile`.
  * I.e., it is enough to obtain the `Makefile` from this dir, the rest is magic.
  * This `Makefile` will `curl` the `CMakeLists.txt` file as needed.
* Builds the code from `src/`.
  * Every `src/*.cc` becomes a binary.
  * Every `src/lib_*.cc` becomes a static library.
  * Every `src/test_*.cc` becomes a test target.
  * Every `src/dlib_*.cc` becomes a shared `.{so|dylib}` library.
  * All static libraries are linked to all binaries and tests for this simple setup.
    * Linking static libraries to shared libraries is still a `TODO(dkorolev)`.
* Defines many useful `make` targets:
  * `clean`, `debug`, `release`, `test`, `release_test`, `clean`, `fmt`, etc.
* Uses `current` and `googletest` from `..` or `../..` if they exist.
  * Clones `current` and `googletest` if needed.
  * Adds them into `.gitignore` if they are cloned into the local dir.

The `test/` directory in this repo illustrates what it takes to use this `Makefile` + `CMakeLists.txt`.

Please refer to [`run-cmake-test.sh`](https://github.com/c5t/Current/blob/stable/cmake/run-cmake-test.sh) for a comprehensive end-to-end test for the above.
