# Title

FNCAS is not Computer Algebra System

# Purpose

Clear, but yet to be put here.

# Build Instructions

To run on a fresh Ubuntu machine run the following.

    sudo apt-get update
    sudo apt-get install -y git build-essential libboost-dev nasm clang
    
    git clone https://github.com/dkorolev/fncas.git

    cd fncas
    (cd fncas; make)
    (cd test; make)
    cd -
    
* (cd fncas; make) confirms the build environment is set up.
* (cd test; make) runs some basic tests.

## Issues

If you get an error like this when compiling with clang++:

    /usr/include/c++/4.6/chrono:666:7: error: static_assert expression is not an integral constant expression

ust comment out that static_assert.
