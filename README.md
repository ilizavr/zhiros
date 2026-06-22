это жирОС


как запустить:

1.создать рамдиск `dd if=/dev/zero of=iso/boot/initrd.img bs=10M count=1`

2.форматировать его в fat16 `mkfs.fat -F16 iso/boot/initrd.img`

3.1.скомпилировать запустить ОС в эмуляторе. `make clean build run`

3.2.собрать образ для реального железа `make clean build build_grub`
