#!/bin/bash

export INSTALL_PATH=$HOME/install-ref
export BIN_PATH=$HOME/bin-ref
export PKG_CONFIG_PATH=$INSTALL_PATH/lib/pkgconfig:$INSTALL_PATH/share/pkgconfig
export LD_LIBRARY_PATH=$INSTALL_PATH/lib
export PATH=$BIN_PATH:$PATH
#export PKG_CONFIG_PATH=${INSTALL_PATH}/lib/pkgconfig:${INSTALL_PATH}/share/pkgconfig
