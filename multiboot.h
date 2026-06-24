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
	u32 drivers_length;
	u32 drivers_addr;
	u32 config_table;
	u32 boot_loader_name;
	u32 apm_table;

	u32 vbe_control_info;
	u32 vbe_mode_info;
	u16 vbe_mode;
	u16 vbe_interface_seg;
	u16 vbe_interface_off;
	u16 vbe_interface_len;

	u64 framebuffer_addr;
	u32 framebuffer_pitch;
	u32 framebuffer_width;
	u32 framebuffer_height;
	u8 framebuffer_bpp;
	u8 framebuffer_type;
};
__attribute__((packed)) struct multiboot_mod_list
{
        u32 mod_start;
        u32 mod_end;
        u32 cmd_line;
        u32 pad;
};

short test_video_buffer[80*25+0x40];

void _multiboot_entry(struct multiboot_info* mbi,u32 magic,u8 codesegment){
	if(mbi->framebuffer_type==1){
		video = test_video_buffer+0x20;
		fbdev_init(mbi->framebuffer_addr,mbi->framebuffer_width,mbi->framebuffer_height,mbi->framebuffer_pitch,mbi->framebuffer_bpp);		
	}
	code_segment = codesegment;//for interrupts

	if(magic != 0x2BADB002) KLOGF("not multiboot magic");
	if((mbi->flags&(1<<6)) == 0) KLOGF("mmap not given by multiboot");
	if(mbi->mods_count<1) KLOGF("multiboot dont give initial ramdisk");
	if(mbi->mods_count>1) KLOGF("multiboot give so many modules");
	KLOGI("multiboot started kernel. cmdline: ");
	if(mbi->flags&(1<<2))print((char*)mbi->cmdline);
	print("\n    kernel at 0x");
	print_hex((u32)_kernel_start);
	print("-0x");
	print_hex((u32)_kernel_end);
	print("\n");

	struct multiboot_mod_list* mods = (struct multiboot_mod_list *)mbi->mods_addr;
	
	print("    ramdisk at 0x");
	print_hex((u32)mods->mod_start);
	print("-0x");
	print_hex((u32)mods->mod_end);
	print("\nbootloader name:");
	
	print((char*)mbi->boot_loader_name);
	print("\n");

	ramdisk_size = mods->mod_end;
	init_alloc_multiboot(mbi->mmap_addr,mbi->mmap_len,mods->mod_end);
	
        add_ram_disk("initrd",(char*)mods->mod_start,mods->mod_end-mods->mod_start);

	main();
}
