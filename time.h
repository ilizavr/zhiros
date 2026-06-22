#define CMOS_SEC 0x00
#define CMOS_MIN 0x02
#define CMOS_HOUR 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09
#define CMOS_STAT_A 0x0A
#define CMOS_STAT_B 0x0B

struct rtc_time
{
	u8 second;
	u8 minute;
	u8 hour;
	u8 day;
	u8 month;
	u16 year;
};

u8 read_cmos(u8 reg)
{
	port_byte_out(0x70, reg);
	return port_byte_in(0x71);
}

bool is_rtc_updating()
{
	return (read_cmos(CMOS_STAT_A) & 0x80);
}

u8 bcd_to_bin(u8 bcd)
{
	return ((bcd/16)*10)+(bcd&0xf);
}

struct rtc_time get_time()
{
	struct rtc_time ret;
	while(is_rtc_updating()){}
	ret.second = read_cmos(CMOS_SEC);
        ret.minute = read_cmos(CMOS_MIN);
        ret.hour = read_cmos(CMOS_HOUR);
        ret.day = read_cmos(CMOS_DAY);
        ret.month = read_cmos(CMOS_MONTH);
        ret.year = read_cmos(CMOS_YEAR);

	u8 stateb = read_cmos(CMOS_STAT_B);
	if(!(stateb & 0x04)){
		ret.second = bcd_to_bin(ret.second);
                ret.minute = bcd_to_bin(ret.minute);
                ret.hour = bcd_to_bin(ret.hour);
                ret.day = bcd_to_bin(ret.day);
                ret.month = bcd_to_bin(ret.month);
                ret.year = bcd_to_bin(ret.year);
	}

	return ret;
}

extern void timer_isr_handler();

void init_timer()
{
	u32 divisor = 1193182/2000;
	
	outb(0x43, 0x36);
	
	outb(0x40, divisor);
	outb(0x40, divisor>>8);
	
	set_idt_gate(0x20,(u32)timer_isr_handler);
}
u32 timerticks = 0;

void sleep(u32 milisecs)
{
	timerticks = 0;
	while(timerticks<milisecs)
	{
		asm volatile("hlt");
	}
}
