#!/bin/sh

set -eu

echo $$ > seed.json
echo "Seed for random number generation: $$"

../simplecolormap simple > colormap.json

cat offsets |
while read M
do
    N="offset_$(echo $M | sed 's/[[-]//g' | sed 's/,/ /g' | cut -d ' ' -f 1)"
    echo $N
    echo "\"$N.tiff\"" > imagename.json
    echo "\"$N\"" > modelname.json
    echo $M > offset_min.json
    echo $M | sed 's/-//g' > offset_max.json
    datalackey-make tgt -m $@
done

rm -f seed.json colormap.json offset_min.json offset_max.json imagename.json modelname.json
