#!/bin/bash
cd build-spice-protocol
../spice-protocol-0.12.6/configure --prefix=$INSTALL_PATH
make
sudo make install
