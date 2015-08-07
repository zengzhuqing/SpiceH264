#!/bin/bash
cd build-spice 
../spice-0.12.4/configure --prefix=$INSTALL_PATH --disable-celt051 --disable-silent-rules \
		--disable-smartcard --enable-client
make
sudo make install
