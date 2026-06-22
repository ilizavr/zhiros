u32 next=0;

u32 rand(){
	next = (next+ticks) * 1103515245+12345-ticks;
	return (next>>16)&0x7FFF;
}

