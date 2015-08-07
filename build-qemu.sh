#!/bin/bash
cd build-qemu 
../qemu-2.3.0/configure --prefix=$INSTALL_PATH --enable-kvm --enable-debug --enable-spice --target-list=i386-softmmu
make
sudo make install
