#!/bin/bash
BUILD_DIR=build-spice-gtk
if [ ! -d $BUILD_DIR ];then
    mkdir $BUILD_DIR
fi
#Need to install perl-Text-CSV_XS.x86_64 before
#Need to install Text::CSV perl module by `perl -MCPAN -e'install Text::CSV'`
cd $BUILD_DIR 
../spice-gtk3-0.22/configure --prefix=$INSTALL_PATH --bindir=$BIN_PATH --with-gtk=3.0 --enable-vala 
make
sudo make install
