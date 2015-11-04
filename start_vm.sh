QEMU=/home/zengzhuqing/bin/qemu-system-x86_64
IMAGES=/mnt/win7.img
#sudo $QEMU -m 1024 -drive file=images/test.img,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -net nic -net tap,script=no -vnc :1 -spice port=7001,disable-ticketing -enable-kvm -smp 1 -usbdevice tablet -soundhw all --full-screen -monitor stdio -vga qxl -global qxl-vga.ram_size=67108865 -global qxl-vga.vram_size=67108864 

USB_PARAM="-device ich9-usb-ehci1,id=usb \
-device ich9-usb-uhci1,masterbus=usb.0,firstport=0,multifunction=on \
-device ich9-usb-uhci2,masterbus=usb.0,firstport=2 \
-device ich9-usb-uhci3,masterbus=usb.0,firstport=4 \
-chardev spicevmc,name=usbredir,id=usbredirchardev1 \
-device usb-redir,chardev=usbredirchardev1,id=usbredirdev1 \
-chardev spicevmc,name=usbredir,id=usbredirchardev2 \
-device usb-redir,chardev=usbredirchardev2,id=usbredirdev2 \
-chardev spicevmc,name=usbredir,id=usbredirchardev3 \
-device usb-redir,chardev=usbredirchardev3,id=usbredirdev3"
$QEMU -m 2048 -drive file=$IMAGES,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -enable-kvm -smp 4 -monitor stdio -vga qxl -spice port=7002,disable-ticketing $USB_PARAM
