QEMU=qemu-system-i386
$QEMU -m 1024 -drive file=images/test.img,cache=writeback,if=virtio -net nic -net tap,script=no -vnc :1 -spice port=7001,disable-ticketing -enable-kvm -smp 1 -usbdevice tablet -soundhw all --full-screen
