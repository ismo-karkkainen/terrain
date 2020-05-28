#!/bin/sh

if [ $# -lt 6 ]; then
    echo "Usage: $(basename $0) program input lowr highr lowo higho [args...]"
    exit 2
fi

PROG=$1
IN=$2
LOWR=$3
HIGHR=$4
LOWO=$5
HIGHO=$6
shift 6

$PROG $@ < $IN > $IN.tmp
within $IN.tmp 0 0 1 && within $IN.tmp 1 0 1 && within $IN.tmp 2 $LOWR $HIGHR && within $IN.tmp 3 $LOWO $HIGHO
STATUS=$?
rm -f $IN.tmp
exit $STATUS
