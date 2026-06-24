bits 32

MULTIBOOT_VIDEO_MODE equ 4

section .multiboot
	align 4
	dd 0x1BADB002
	dd MULTIBOOT_VIDEO_MODE
	dd -(0x1BADB002+MULTIBOOT_VIDEO_MODE)
	
	;jmp _start
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
	dd 640
	dd 400
	dd 0

section .text
global _start
global keyboard_isr_handler
global _none_interrupt
global timer_isr_handler

extern _multiboot_entry
extern keyboard_handler
extern timerticks
extern ticks
extern pic_eoi

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
timer_isr_handler:
	pusha
	add dword [timerticks],10
	add dword [ticks],10
	call pic_eoi
	popa
	iret

section .bss
resb 8192
stack:


