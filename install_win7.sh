IMAGES=/mnt/win7.img
ISO=/home/zengzhuqing/cn_windows_7_professional_x64_dvd_x15-65791.iso 
/home/zengzhuqing/bin/qemu-system-x86_64 -m 4096 -drive file=$IMAGES,if=none,id=drive-ide0-0-0,format=raw,cache=none -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive if=none,id=drive-ide0-1-0,readonly=on,format=raw -net nic -net tap,script=no -enable-kvm -smp 1 -usbdevice tablet -soundhw all --full-screen -cdrom $ISO 
