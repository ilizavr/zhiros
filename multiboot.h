__attribute__((packed)) struct multiboot_info
{
	u32 flags;
	u32 mem_lower;
	u32 mem_upper;
	u32 boot_device;
	u32 cmdline;
	u32 mods_count;
	u32 mods_addr;
	u32 num;
	u32 size;
	u32 addr;
	u32 shndx;
	u32 mmap_len;
	u32 mmap_addr;
};

void _multiboot_entry(struct multiboot_info* mbi,u32 magick){
	if(magick != 0x2BADB002) KLOGF("not multiboot magick");
	if((mbi->flags&(1<<6)) == 0) KLOGF("mmap not given by multiboot");
	KLOGI("multiboot started kernel. cmdline: ");
	print((char*)mbi->cmdline);
	print("\n    kernel at 0x");
	print_hex((u32)_kernel_start);
	print("-0x");
	print_hex((u32)_kernel_end);
	print("\n");
	
	
	init_alloc_multiboot(mbi->mmap_addr,mbi->mmap_len);

	main();
}
