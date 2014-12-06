# gtest

A header-only port of http://code.google.com/p/googletest/

Based on version 1.7.0, http://googletest.googlecode.com/files/gtest-1.7.0.zip

Original ```LICENSE```, ```COPYING```, ```CONTRIBUTORS``` and ```README``` files kept.

Files taken are ```src/``` and ```include/```. Instructions:

```
unzip gtest-1.7.0.zip
cp -rv gtest-1.7.0/include gtest-1.7.0/src .
find include/gtest/internal/ -maxdepth 1 -type f | xargs sed -i "s/#include \"gtest\/internal\//#include \"/g"
find include/gtest/internal/ -maxdepth 1 -type f | xargs sed -i "s/#include \"gtest\//#include \"\.\.\//g"
find include/gtest/          -maxdepth 1 -type f | xargs sed -i "s/#include \"gtest\//#include \"/g"
find src/                    -maxdepth 1 -type f | xargs sed -i "s/#include \"src\//#include \"/g"
find src/                    -maxdepth 1 -type f | xargs sed -i "s/#include \"src\//#include \"/g"
find src/                    -maxdepth 1 -type f | xargs sed -i "s/#include \"gtest\//#include \"\.\.\\/include\/gtest\//g"
```

Ported by Dmitry ("Dima") Korolev with help from Alex Zolotarev from Belarus.
