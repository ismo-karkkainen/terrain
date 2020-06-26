#!/bin/sh

set -eu

echo $$ > seed.json
echo "Seed for random number generation: $$"

../simplecolormap simple > colormap.json

cat counts |
while read M
do
    N="count_$M"
    echo $N
    echo "\"$N.tiff\"" > imagename.json
    echo "\"$N\"" > basename.json
    echo "$M" > count.json
    datalackey-make write -m $@
done

rm -f seed.json colormap.json count.json imagename.json basename.json
