OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS {
	. = 0x100000;
	_kernel_start = .;
	.text : { *(.text) }
	.data : { *(.data) }
	.bss : { *(.bss) }
	_kernel_end = .;
}
