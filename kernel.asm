bits 32

section .text

	align 4
	dd 0x1BADB002
	dd 0x00
	dd -(0x1BADB002+0x00)

global _start
global keyboard_isr_handler
global _none_interrupt

extern _multiboot_entry
extern keyboard_handler

_start:
	cli
	mov esp, stack
	
	push cs
	push eax
	push ebx
	call _multiboot_entry

	jmp $


keyboard_isr_handler:
	pusha
	
	call keyboard_handler

	popa
_none_interrupt:
	iret
section .bss
resb 8192
stack:


