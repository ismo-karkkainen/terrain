#!/bin/sh

set -eu
sudo apt-get update >/dev/null
sudo apt-get install -y -q cmake make clang ruby >/dev/null
$1/test/port/clang-build.sh $1
