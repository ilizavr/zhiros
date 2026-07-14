#define VGA_CTRL_REGISTER 0x3d4
#define VGA_DATA_REGISTER 0x3d5
#define VGA_OFFSET_LOW 0x0f
#define VGA_OFFSET_HIGH 0x0e

void *kalloc(u32 size);
void KLOGF(char *string);
void print(char * string);

u8 WIDTH = 80,HEIGHT=25;

void *memset(void *s, int c, size_t n) {
        unsigned char *p = s;
        while (n--) *p++ = (unsigned char)c;
        return s;
}
void *memcpy(void *dest, const void *src, size_t n) {
        unsigned char *d = dest;
        const unsigned char *s = src;
        while (n--) *d++ = *s++;
        return dest;
}
void *memmove(void *dest, const void *src, size_t n) {
        unsigned char *d = dest;
        const unsigned char *s = src;
        if (d < s) {
                while (n--) *d++ = *s++;
        } else {
                d += n; s += n;
                while (n--) *--d = *--s;
        }
        return dest;
}
int memcmp(const void *s1, const void *s2, size_t n) {
        const unsigned char *p1 = s1, *p2 = s2;
        while (n--) {
                if (*p1 != *p2) return *p1 - *p2;
                p1++; p2++;
        }
        return 0;
}


size_t strlen(const char *s) {
        size_t len = 0;
        while (*s++) len++;
        return len;
}

char* strcat(char* s1,char* s2) {
int len1 = strlen(s1);
int len2 = strlen(s2);

char* new_str = (char*)kalloc(len1 + len2 + 1);

int i = 0;
int j = 0;

while(i < len1){
new_str[i] = s1[i];
++i;
}


++i;

while(j < len2){
new_str[i] = s2[j];
++j;
++i;
}

++i;

new_str[i] = '\0';

return new_str;
}

void assert(bool expression,char* msg){
if(!expression){
KLOGF("assertion failed because of ");
print(msg);
print("\n");
}
}

int strncmp(const char *s1, const char *s2, size_t n) {
        while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
        if (n == 0) return 0;
        return *(unsigned char *)s1 - *(unsigned char *)s2;
}
char *strchr(const char *s, int c) {
        while (*s) {
                if (*s == (char)c) return (char *)s;
                s++;
        }
        return 0;
}
int strcmp(const char *s1, const char *s2) {
        while (*s1 && (*s1 == *s2)) { s1++; s2++; }
        return *(unsigned char *)s1 - *(unsigned char *)s2;
}


char *strcpy(char *dest, const char *src)
{
    char *saved = dest;
    while ((*dest++ = *src++))
        ;
    return saved;
}


void memmove_short(short *dst, short *src, int size)
{
    for (int i = 0; i < size; i++)
    {
        dst[i] = src[i];
    }
}


void memset_short(short *dst, short sym, int size)
{
    for (int i = 0; i < size; i++)
    {
        dst[i] = sym;
    }
}

bool is_dec_number(char *str)
{
	while(*str){
		if(*str>'9'||*str<'0') return false;
		str++;
	}
	return true;
}
bool is_hex_number(char *str)
{
	if(str[0] != '0') return false;
	if(str[1] != 'x') return false;
	str+=2;
	while(*str){
		if((*str>'9'||*str<'0')&&(*str<'a'||*str>'z'))return false;
		str++;
	}
	return true;
}

u32 str2int(char *str)
{	
	int ret = 0;
	int len = strlen(str);
	
	if(is_dec_number(str)){
		for(int i = 0;i<len;i++)
		{
			ret=ret*10+str[i]-'0';
		}
	}else if(is_hex_number(str)){
		for(int i = 2;i<len;i++){
			char sym = str[i];
			if(sym<'9')ret=ret*16+str[i]-'0';
			else ret = ret*16+str[i]-'a'+0xA;
		}
	}

	return ret;
}

void __set_cursor_offset(short offset)
{
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
	port_byte_out(VGA_DATA_REGISTER, offset&0xFF);
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
	port_byte_out(VGA_DATA_REGISTER, offset>>8);
}

short __get_cursor_offset() 
{
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
	short offset = port_byte_in(VGA_DATA_REGISTER);
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
	offset |= port_byte_in(VGA_DATA_REGISTER)<<8;
	return offset;
}
void cls()
{
	__set_cursor_offset(0);
	memset_short(video,' '|0b0111<<8,WIDTH*HEIGHT);
}
void roll_up(short *current_pos)
{
	memmove_short(video,video+WIDTH,(HEIGHT-1)*WIDTH);
	memset_short(video+(HEIGHT-1)*WIDTH,' ' | (0b0111<<8) ,WIDTH);
	*current_pos-=WIDTH;
}
bool is_interrupt_enabled=false;

void pic_eoi();

void print_color(char * string, char color)
{
	asm volatile("cli");
	serial_print(string);
	short current_position = __get_cursor_offset();

	for(int i = 0;;i++,current_position++)
	{	
		int y = current_position/WIDTH;
		if(y>=HEIGHT)roll_up(&current_position);

		if(!string[i]) break;

		int x = current_position%WIDTH;
		if (string[i]=='\n') current_position=current_position-x-1+WIDTH;
		else video[current_position] = string[i] | (color<<8);

	}
	__set_cursor_offset(current_position);
	if(is_interrupt_enabled) {
		pic_eoi();
		asm volatile("sti");
	}
}

void putchar(char chr)
{
	asm volatile("cli");
	
	serial_putc(chr);	

	short current_position = __get_cursor_offset();
	
	int x = current_position%WIDTH;
	if (chr=='\n') current_position=current_position-x-1+WIDTH;
	else video[current_position] = chr | (0b0111<<8);

	current_position++;

	int y = current_position/WIDTH;
       	if(y>=HEIGHT)roll_up(&current_position);

        __set_cursor_offset(current_position);
	
	if(is_interrupt_enabled) {
		pic_eoi();
		asm volatile("sti");
	}
}

void print(char * string)
{
	print_color(string,0b0111);
}

void KLOGE(char * string)
{
	print_color("[ERROR]",0b0100);
	print(string);
}
void KLOGI(char * string)
{
	print_color("[OK]",0b0010);
	print(string);
}
void KLOGW(char * string)
{
	print_color("[WARNING]",0b0110);
	print(string);
}
void KLOGF(char *string)
{
	print_color("[FATAL]",0b1100);
	print(string);
	asm volatile("cli");
	asm volatile("hlt");
}
void print_hex(u32 num)
{
	char buffer[12]; buffer[11] = '\0';buffer[10] = '0';
	int i = 10;
	if(num==0)i--;
	for(;num>0;i--)
	{
		u64 nn = num %16;
		if(nn<10) buffer[i]=nn+'0';
		else buffer[i] = nn-0xa+'A';
		num>>=4;
	}
	print(buffer+i+1);
}

void print_int(u32 num)
{
	char buffer[12]; buffer[11] = '\0';buffer[10] = '0';
	int i = 10;
	if(num==0)i--;
	for(;num>0;i--)
	{
		buffer[i] = num%10 +'0';
		num/=10;
	}
	
	print(buffer+i+1);
}

void hexdump(char *data, u32 size)
{
    u8 *ptr = (u8*)data;
    const char* hex_digits= "0123456789ABCDEF";

    for (u32 i = 0; i < size; i += 16)
    {
        u32 chunk = size - i;
        if (chunk > 16) chunk = 16;

        for (int j = 28; j >= 0; j -= 4)
        {
            putchar(hex_digits[(i >> j) & 0x0F]);
        }
        print(": ");

        for (u32 j = 0; j < 16; j++)
        {
            if (j < chunk)
            {
                u8 b = ptr[i + j];
                putchar(hex_digits[b >> 4]);
                putchar(hex_digits[b & 0x0F]);
            }
            else
            {
                print("  ");
            }
            putchar(' ');
            if (j == 7) print(" ");
        }

        print(" |");

        for (u32 j = 0; j < chunk; j++)
        {
            u8 b = ptr[i + j];
            if (b >= 32 && b <= 126)
            {
                putchar(b);
            }
            else
            {
                putchar('.');
            }
        }
        print("|\n");
    }
}
