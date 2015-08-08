#!/bin/bash
cd build-spice 
../spice-0.12.4/configure --prefix=$INSTALL_PATH --disable-celt051 --disable-silent-rules \
		--disable-smartcard --enable-client 
make
sudo make install
sudo cp /usr/lib/libspice-server.so.1.8.0 /usr/lib/i386-linux-gnu/libspice-server.so.1.8.0
sudo rm /usr/lib/libspice-server*
