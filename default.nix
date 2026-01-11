{ pkgs ? import <nixpkgs> {} }:

pkgs.stdenv.mkDerivation {
  pname = "жирос";
  version = "1";
  src = ./.;

  nativeBuildInputs = with pkgs; [
    nasm
    clang
    lld
    xorriso
    grub2
    mtools
    libisoburn
  ];

  hardeningDisable = [ "all" ];

  buildPhase = ''
    nasm -f elf32 kernel.asm -o kernel_asm.o
    clang -m32 -ffreestanding -fno-builtin -fno-exceptions -nostdlib -c kernel.c -o kernel_c.o
    ld.lld -m elf_i386 -T linker.ld -o kernel.bin kernel_asm.o kernel_c.o
  '';

  installPhase = ''
    mkdir -p $out
    mkdir -p iso/boot
    cp kernel.bin iso/boot/kernel.bin
    grub-mkrescue -o $out/zhirOS.iso iso
    cp kernel.bin $out/kernel.bin
  '';
}
