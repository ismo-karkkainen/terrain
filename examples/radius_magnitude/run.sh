#!/bin/sh

set -eu

echo $$ > seed.json
echo "Seed for random number generation: $$"

../simplecolormap simple > colormap.json

cat radiuses |
while read M
do
    N="radius_$(echo $M | sed 's/[[]//g' | sed 's/,/ /g' | cut -d ' ' -f 1).tiff"
    echo $N
    echo "\"$N\"" > imagename.json
    echo $M > radius.json
    datalackey-make tgt -m $@
done

rm -f seed.json colormap.json radius.json imagename.json
