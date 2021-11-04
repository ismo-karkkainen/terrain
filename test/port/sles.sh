#!/bin/sh

set -eu
sudo zypper refresh >/dev/null
sudo zypper install -y cmake make gcc-c++ ruby >/dev/null
$1/test/port/gcc-build.sh $1
