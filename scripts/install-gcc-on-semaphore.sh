#!/bin/bash
#
# Original source:
# https://gist.githubusercontent.com/ervinb/e85fcb1e238199569dfb8452c6883177/raw/gcc-installer-semaphore.sh
#
# Modified for g++-7 by @dkorolev on 6/30/18.

gcc_version=${1:-'7'}
gpp_version=$gcc_version

echo "- Installing packages"
install-package gcc-${gcc_version} g++-${gpp_version}

echo "- Adding configuration"
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${gcc_version} 80 --slave /usr/bin/g++ g++ /usr/bin/g++-${gpp_version}

echo "- Switching version"
change-gcc-version $gcc_version
