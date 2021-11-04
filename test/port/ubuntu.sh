#!/bin/sh

set -eu
sudo apt-get update >/dev/null
sudo apt-get install -y -q cmake build-essential ruby >/dev/null
$1/test/port/gcc-build.sh $1
