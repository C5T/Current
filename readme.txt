/*
# To run on a fresh Ubuntu machine run the following.
sudo apt-get update
sudo apt-get install -y git build-essential libboost-dev nasm clang
git clone https://github.com/dkorolev/fncas.git
# This ensures the project can be built.
(cd fncas/fncas; make)
# This runs the perf test and generates fncas/test/perf/report.html.
(cd fncas/test/perf; ./run_test.sh)
*/

// Ah, and if you get an error like this when compiling with clang++:
// /usr/include/c++/4.6/chrono:666:7: error: static_assert expression is not an integral constant expression
// Just comment out that static_assert.

