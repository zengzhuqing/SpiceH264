#!/bin/bash
prog=bin/rgb2yuv
for f in data/*.bmp;do
    num=`echo $f | awk -F. '{print $1}'`
    out_f=${num}.yuv
    $prog $f $out_f
done
