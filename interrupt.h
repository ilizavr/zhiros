void pic_remap()
{
	//init master and slave
	outb(0x20,0x11);
	outb(0xA0,0x11);
	
	//mov irq to 0x20-0x2F
	outb(0x21,0x20);
	outb(0xA1,0x28);

	//master-slave connection
	outb(0x21,0x04);
	outb(0xA1,0x02);

	//8086 mode
	outb(0x21,0x01);
	outb(0xA1,0x01);

	//enable interrupts
	outb(0x21,0xFD);
	outb(0xA1,0xFF);
}

__attribute__((packed))struct idt_entry
{
	u16 low_offset;
	u16 sel;
	u8 always0;
	u8 flags;
	u16 high_offset;
};
__attribute__((packed))struct idt_ptr
{
	u16 limit;
	u16 base_low;
	u16 base_high;
};

struct idt_entry idt[256];
struct idt_ptr idtp;

u8 code_segment;

extern void _none_interrupt();
void set_idt_gate(u8 num,u32 base)
{
	idt[num].low_offset = base&0xFFFF;
	idt[num].high_offset = (base>>16)&0xFFFF;
	idt[num].sel = code_segment;
	idt[num].always0 = 0;
	idt[num].flags = 0x8E;//ring0 32bit
}

void init_idt()
{
	idtp.limit = 256*sizeof(struct idt_entry)-1;
	idtp.base_low = (u32)&idt&0xFFFF;
	idtp.base_high = (u32)&idt>>16;
	
	for(int i = 0;i<=256;i++) set_idt_gate(i,(u32)_none_interrupt);

	asm volatile("lidt (%0)" : : "r" (&idtp));
}

void pic_eoi()
{
	outb(0x20,0x20);
}
