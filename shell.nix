{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gcc #СЛАВА GCC, гмаил пижор
    lld
    nasm
    gnumake
    xorriso
    qemu
    grub2
  
   vim #егор пошел нахуй. вим топ!
  ];
  shellHook = ''
    export CC=clang
    export LD=ld.lld
    echo "добро пожаловать в среду разработки жирОС"
  '';
}


