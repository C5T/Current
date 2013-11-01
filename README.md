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

### g++ sorry, unimplemented: non-static data member initializers

You'll need gcc/g++ 4.7+. I had to do the following in Ubuntu 12.04:

    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install gcc-4.7 g++-4.7

    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.6 
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.7 40 --slave /usr/bin/g++ g++ /usr/bin/g++-4.7 
    sudo update-alternatives --config gcc

### static_assert in /usr/include/c++/4.6/chrono

If you get an error like this when compiling with clang++:

    /usr/include/c++/4.6/chrono:666:7: error: static_assert expression is not an integral constant expression

Just comment out that static_assert.
