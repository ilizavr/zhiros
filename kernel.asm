
bits 32

section .text

	align 4
	dd 0x1BADB002
	dd 0x00
	dd -(0x1BADB002+0x00)

global _start
extern main

_start:
	cli
	mov esp, stack
	
	call main

	jmp $

section .bss
resb 8192
stack:

