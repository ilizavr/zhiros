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
 dd 0
 dd 0
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
 mov edx, eax
 call init_gdt

 mov esp, stack

 push edx
 push ebx
 call _multiboot_entry

 jmp $

init_gdt:
 lgdt [gdt_descriptor]
 jmp 0x08:.reload_cs

.reload_cs:
 mov ax, 0x10
 mov ds, ax
 mov ss, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 ret

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

gdt:
 dd 0
 dd 0

 ;32bit code
 dw 0xFFFF ; end
 dw 0 ; base low 16bits
 db 0 ; base mid 8 bits
 db 0x9A ; rx
 db 0xCF ; flags
 db 0 ; base high 8 bits
 ;32bit data
 dw 0xFFFF
 dw 0
 db 0
 db 0x92; rw
 db 0xCF
 db 0

 ;16 bit code
 dw 0xFFFF
 dw 0
 db 0
 db 0x9A
 db 0
 db 0
 ;16bit data
 dw 0xFFFF
 dw 0
 db 0
 db 0x92
 db 0
 db 0
gdt_descriptor:
 dw gdt_descriptor-gdt -1
 dd gdt



section .bss
resb 8192
stack:
