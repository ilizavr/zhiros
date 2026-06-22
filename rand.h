u32 next=0;

u32 rand(){
	next = next * 1103515245+12345;
	return (next>>16)&0x7FFF;
}

void srand()
{
	struct rtc_time now = get_time();
	next = (now.second * 1000) + (now.minute * 60) + now.hour;
}
