QEMU=/home/zengzhuqing/bin/qemu-system-x86_64
IMAGES=/mnt/win7.img
#sudo $QEMU -m 1024 -drive file=images/test.img,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -net nic -net tap,script=no -vnc :1 -spice port=7001,disable-ticketing -enable-kvm -smp 1 -usbdevice tablet -soundhw all --full-screen -monitor stdio -vga qxl -global qxl-vga.ram_size=67108865 -global qxl-vga.vram_size=67108864 
$QEMU -m 4096 -drive file=$IMAGES,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -enable-kvm -smp 4 -usbdevice tablet -soundhw all
