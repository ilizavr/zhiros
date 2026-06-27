bits 32

MULTIBOOT_VIDEO_MODE equ 4

section .multiboot
 align 4
 dd 0x1BADB002
 dd MULTIBOOT_VIDEO_MODE
 dd -(0x1BADB002+MULTIBOOT_VIDEO_MODE)

 dd 0
 dd 0
 dd 0
 dd 0
 dd 0
 dd 0
 dd 800
 dd 600
 dd 0

section .text
global _start
global keyboard_isr_handler
global _none_interrupt
global timer_isr_handler
global context_switch

extern _multiboot_entry
extern keyboard_handler
extern timerticks
extern ticks
extern pic_eoi
extern schedule

_start:
 cli
 mov esp, stack

 push cs
 push eax
 push ebx
 call _multiboot_entry

 jmp $
keyboard_isr_handler:
 cli
 pusha
 call keyboard_handler
 popa
 sti
_none_interrupt:
 iret
timer_isr_handler:
 cli
 pusha
 add dword [timerticks],10
 add dword [ticks],10
 call pic_eoi
 call schedule
 popa
 sti
 iret

context_switch:
 push ebp
 push ebx
 push esi
 push edi

 mov eax, [esp+24]
 mov [eax], esp

 mov eax, [esp+20]
 mov esp, eax

 pop edi
 pop esi
 pop ebx
 pop ebp
 
 sti
 
 ret

section .bss
resb 8192
stack:
