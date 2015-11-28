#!/bin/bash
if [ ! -d build-spice-protocol ];then
    mkdir build-spice-protocol
fi
# when change enums.h, please rm build-spice-protocol -rf to update
cd build-spice-protocol
../spice-protocol-0.12.10/configure --prefix=$INSTALL_PATH
make
make install
