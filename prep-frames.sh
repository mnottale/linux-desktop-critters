#! /bin/bash

indir=$1
res=$2

for f in $(find $indir -type f); do
    bn=$(basename $f)
    convert $f -size $res -resize $res ./$bn
    convert $f -size $res -resize $res -flop ./$(echo $bn | sed -e 's/0/r_0/')
done