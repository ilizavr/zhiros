u8 port_byte_in(u16 port)
{
    unsigned char result;
    __asm__("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}
void port_byte_out(u16 port, u8 data)
{
    __asm__("out %%al, %%dx" : : "a"(data), "d"(port));
}

u16 port_word_in(u16 port)
{
    u32 _v;
    __asm__("inw %%dx, %%ax" : "=a"(_v) : "d"(port));
    return _v;
}
void port_word_out(u16 port, u16 data)
{
    __asm__("outw %%ax, %%dx" : : "a"(data), "d"(port));
}

u32 port_dword_in(u16 port)
{
    u32 result;
    __asm__("inl %%dx, %%eax" : "=a"(result) : "d"(port));
    return result;
}
void port_dword_out(u16 port, u32 data)
{
    __asm__("outl %%eax, %%dx" : : "a"(data), "d"(port));
}

static inline void insw(u16 port, void *addr, u32 count)
{
    asm volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}
static inline void outsw(u16 port, const void *addr, u32 count)
{
    asm volatile("rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory");
}
#define inb port_byte_in
#define inl port_dword_in
#define outb port_byte_out
#define outl port_dword_out
