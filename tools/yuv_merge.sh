#!/bin/bash
out_filename=out/out.yuv
for f in data/*.yuv;do
    cat $f
done > $out_filename
