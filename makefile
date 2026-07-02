CC = gcc
ASMC = nasm

CFLAGS = -Wint-conversion -m32 -fno-stack-protector
ASMFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld 

create_rd:
	dd if=/dev/zero   of=iso/boot/initrd.img   bs=1M count=10 
	mkfs.fat -F16 iso/boot/initrd.img
build:
	mkdir build
	nasm bios.asm -o build/bios
	xxd -i build/bios > build/bios_asm_hex.h
	$(CC) $(CFLAGS) -c kernel.c -o build/kernel_c.o
	$(ASMC) $(ASMFLAGS) kernel.asm -o build/kernel_asm.o
	ld $(LDFLAGS) -o build/kernel.bin build/kernel_asm.o build/kernel_c.o
	
	cp build/kernel.bin iso/boot/kernel.bin
build_grub:
	grub-mkrescue -o test.img iso/
run_grub:
	qemu-system-x86_64 -serial stdio -hda test.img -m 1G -netdev user,id=net0 -device e1000,netdev=net0
run:
	qemu-system-x86_64 -serial stdio -kernel build/kernel.bin -initrd iso/boot/initrd.img -m 1G -netdev user,id=net0 -device e1000,netdev=net0
clean:
	rm -rf build
	rm -f test.img
	rm -f iso/boot/kernel.bin
