[ORG 0x7000]
[bits 32]

bios_asm_headers:
 dw bioslba
 dw 0
 dd end





bioslba:
 mov al, [esp+4]
 mov [op],al
 mov al, [esp+8]
 mov [disk],al
 pushad
 sidt [saved_idt]
 mov [saved_esp], esp
 
 jmp 0x18:.pm16

[bits 16]
.pm16:
 mov bx, 0x20
 mov ds, bx
 mov es, bx
 mov ss, bx


 mov eax, cr0
 and eax, ~1
 mov cr0, eax

 jmp 0:.rm16
.rm16:
 xor bx,bx
 mov ds, bx
 mov es, bx
 mov fs, bx
 mov gs, bx
 mov ss, bx
 mov sp,0x6ff8
 lidt [real_idt]
 
 mov si, end
 mov al, 0
 mov ah, [op]
 mov dl, [disk]
 
 sti
 int 0x13
 cli
 
 mov eax, cr0
 or eax, 1
 mov cr0,eax
 
 jmp 0x8:.ret 

[bits 32]
.ret:
 mov bx, 0x10
 mov ds, bx
 mov es, bx
 mov fs, bx
 mov gs, bx
 mov ss, bx
 
 mov esp,[saved_esp]
 lidt [saved_idt]

 popad
 ret

real_idt:
 dw 0x3FF
 dd 0

saved_idt: dq 0
saved_esp: dd 0 

op: db 0
disk: db 0

align 16
end:
