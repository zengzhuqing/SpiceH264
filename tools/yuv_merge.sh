#!/bin/bash
out_filename=out.yuv
for f in data/1024x768/*.yuv;do
    cat $f
done > out/1024x768_$out_filename
for f in data/800x600/*.yuv;do
    cat $f
done > out/800x600_$out_filename
