#define COM1_PORT 0x3F8

void init_serial()
{
	outb(COM1_PORT+1,0);//disable interrupt
	outb(COM1_PORT+3,0x80);//enable DLAB
	
	outb(COM1_PORT,0x03);//38400 bod
	outb(COM1_PORT+1,0);

	outb(COM1_PORT+3,0x03);
	
	outb(COM1_PORT+2,0xC7);
	outb(COM1_PORT+4,0x0B);

	outb(COM1_PORT+4,0x0B);
}

bool is_transmit_empty()
{
	return inb(COM1_PORT+5)&0x20;
}

bool is_signal_recieved()
{
	return inb(COM1_PORT+5)&1;
}

void serial_putc(char chr)
{
	while(!is_transmit_empty()) asm volatile("pause");
	outb(COM1_PORT,chr);
}

char serial_getch()
{
	while(!is_signal_recieved()) asm volatile("pause");
	return inb(COM1_PORT);
}

void serial_print(const char *str)
{
	while(*str)
	{
		serial_putc(*str);
		str++;
	}
}


