QEMU=/home/zengzhuqing/bin/qemu-system-x86_64
IMAGES=/mnt/win7.img
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
$QEMU -m 1024 -drive file=$IMAGES,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -vnc :1 -spice port=7001,disable-ticketing -enable-kvm -smp 2 $USB_PARAM -usbdevice tablet -soundhw all --full-screen -monitor stdio -vga qxl -global qxl-vga.ram_size=67108865 -global qxl-vga.vram_size=67108864

#$QEMU -m 2048 -drive file=$IMAGES,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -enable-kvm -smp 4 -monitor stdio -vga qxl -spice port=7001,disable-ticketing -device virtio-serial-pci,id=virtio-serial0,bus=pci.0,addr=0x6 -chardev pty,id=charserial0 -device isa-serial,chardev=charserial0,id=serial0 -chardev spicevmc,id=charchannel0,name=vdagent -device virtserialport,bus=virtio-serial0.0,nr=1,chardev=charchannel0,id=channel0,name=com.redhat.spice.0 -device usb-tablet,id=input0 $USB_PARAM
