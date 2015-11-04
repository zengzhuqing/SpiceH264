#!/bin/bash

find . -name "autom4te.cache" | xargs rm -rf
find spice-gtk-0.30/ -name "*~" | xargs rm
find spice-gtk-0.30/ -name "tags" | xargs rm
