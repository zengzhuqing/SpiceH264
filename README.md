# SpiceH264
Modify Spice to support H264 streaming based Remote Connection for Desktop Virtualization System

Getting Start:
1.run "source config-env.sh", then run build-qemu.sh, build-spice-protocol.sh, build-libspice.sh to compile and install modified qemu and spice;
2.run start_vm.sh to start a vm in qemu with spice;
3.run "spicy -p 7001" to start a spice client

About Source code:
1.qemu-2.3.0: qemu source code dir;
2.spice-0.12.4: spice source code dir;

About Tools:
1.offline color space transfer: rgb to yuv;
2.offline h264 encoder: yuv to h264 
