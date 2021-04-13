#!/bin/sh

set -eu
sudo yum install -y -q cmake make clang ruby rake
$1/test/port/clang-build.sh $1
