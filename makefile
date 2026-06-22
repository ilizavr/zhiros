CC = gcc
ASMC = nasm

CFLAGS = -m32 -fno-stack-protector
ASMFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld 


build:
	mkdir build
	$(CC) $(CFLAGS) -c kernel.c -o build/kernel_c.o
	$(ASMC) $(ASMFLAGS) kernel.asm -o build/kernel_asm.o
	ld $(LDFLAGS) -o build/kernel.bin build/kernel_asm.o build/kernel_c.o
	cp build/kernel.bin iso/boot/kernel.bin

build_grub:
	grub-mkrescue -o test.img iso/

run:
	qemu-system-x86_64 -serial stdio -kernel build/kernel.bin -initrd iso/boot/initrd.img -m 1G -netdev user,id=net0 -device e1000,netdev=net0

clean:
	rm -rf build
	rm -f test.img
	rm -f iso/boot/kernel.bin
