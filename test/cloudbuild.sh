#!/bin/sh

set -u
export D=$1
R=$2

C="gem install edicta specificjson"
echo "$C" && $C
cd $R

for X in clang++ g++
do
    export X
    mkdir build
    (
        echo "Build $(cat _logs/commit.txt) on $D using $X at $(date '+%Y-%m-%d %H:%M')"
        (
            set -eu
            cd build
            CXX=$X cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
            make -j 2
            make test
        )
        echo "Build and test exit code: $?"
    ) 2>&1 | tee -a "$R/_logs/$D-$X.log"
    rm -rf build
done
