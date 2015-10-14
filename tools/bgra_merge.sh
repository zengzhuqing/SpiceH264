#!/bin/bash
out_filename=out/out.rgb
for f in data/*.raw;do
    cat $f
done > $out_filename
