#!/bin/sh

set -eu
sudo yum install -y -q cmake make gcc-c++ ruby rake
$1/test/port/gcc-build.sh $1
