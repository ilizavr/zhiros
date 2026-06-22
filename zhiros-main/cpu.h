void cpuid(u32 code, u32*a, u32*b, u32*c, u32*d)
{
	asm volatile("cpuid"
		: "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
		: "a" (code));
}

void get_cpu_model(char * buffer)
{
	u32 *ptr = (u32*)buffer;
	u32 a,b,c,d;
	for(u32 i = 0;i<3;i++){
		cpuid(0x80000002+i, &a,&b,&c,&d);
		ptr[0]=a;
		ptr[1]=b;
		ptr[2]=c;
		ptr[3]=d;
		ptr+=4;
	}
	buffer[48] = 0;
}
