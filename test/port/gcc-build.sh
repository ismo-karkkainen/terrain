#!/bin/sh
set -eu

clone () {
    git clone --branch master --depth 1 $1 >/dev/null
}

clone https://github.com/onqtam/doctest.git
mv doctest dtroot
mv dtroot/doctest .
rm -rf dtroot
clone https://github.com/ismo-karkkainen/edicta.git
cd edicta
sudo rake install >/dev/null
cd ..
clone https://github.com/ismo-karkkainen/specificjson.git
mkdir sjbuild
cd sjbuild
ln -s ../doctest doctest
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../specificjson >/dev/null
make -j 3 >/dev/null
sudo make install >/dev/null
cd ..
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release $1
make -j 3
make test
