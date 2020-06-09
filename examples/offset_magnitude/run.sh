#!/bin/sh

set -eu

echo $$ > seed.json
echo "Seed for random number generation: $$"

../simplecolormap simple > colormap.json

cat offset_mins |
while read M
do
    N="offset_$(echo $M | sed 's/[[-]//g' | sed 's/,/ /g' | cut -d ' ' -f 1).tiff"
    echo $N
    echo "\"$N\"" > imagename.json
    echo $M > offset_min.json
    echo $M | sed 's/-//g' > offset_max.json
    datalackey-make tgt -m $@
done

rm -f seed.json colormap.json offset_min.json offset_max.json imagename.json
