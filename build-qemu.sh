#!/bin/bash
if [ ! -d build-qemu ];then
    mkdir build-qemu
fi
cd build-qemu 
../qemu-1.5.3/configure --prefix=$INSTALL_PATH --bindir=$BIN_PATH --enable-kvm --enable-debug --enable-spice --enable-usb-redir --target-list="i386-softmmu x86_64-softmmu"

make
make install
