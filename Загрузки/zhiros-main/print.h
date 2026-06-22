short *video = (short *)0xB8000;

const char WIDTH = 80, HEIGHT = 25;

#define VGA_CTRL_REGISTER 0x3d4
#define VGA_DATA_REGISTER 0x3d5
#define VGA_OFFSET_LOW 0x0f
#define VGA_OFFSET_HIGH 0x0e

int strlen(const char *s)
{
    int i = 0;
    while (*(s++))
        i++;
    return i;
}

char *strcpy(char *dest, const char *src)
{
    char *saved = dest;
    while ((*dest++ = *src++))
        ;
    return saved;
}

void memcpy(char *dst, char *src, int size)
{
    while (size > 0)
    {
        *(dst++) = *(src++);
        size--;
    }
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return (u8)*s1 - (u8)*s2;
}

int memcmp(const void *s1, const void *s2, unsigned int n)
{
    const u8 *p1 = s1;
    const u8 *p2 = s2;
    for (unsigned int i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
        {
            return p1[i] - p2[i];
        }
    }
    return 0;
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

u32 str2int(char *str)
{
    int ret = 0;
    int len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        ret = ret * 10 + str[i] - '0';
    }

    return ret;
}

void __set_cursor_offset(short offset)
{
    port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
    port_byte_out(VGA_DATA_REGISTER, offset & 0xFF);
    port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
    port_byte_out(VGA_DATA_REGISTER, offset >> 8);
}

short __get_cursor_offset()
{
    port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
    short offset = port_byte_in(VGA_DATA_REGISTER);
    port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
    offset |= port_byte_in(VGA_DATA_REGISTER) << 8;
    return offset;
}
void cls()
{
    __set_cursor_offset(0);
    memset_short(video, ' ' | 0b0111 << 8, WIDTH * HEIGHT);
}
void roll_up(short *current_pos)
{
    memmove_short(video, video + WIDTH, (HEIGHT - 1) * WIDTH);
    memset_short(video + (HEIGHT - 1) * WIDTH, ' ' | (0b0111 << 8), WIDTH);
    *current_pos -= WIDTH;
}
void print_color(char *string, char color)
{
    short current_position = __get_cursor_offset();

    for (int i = 0;; i++, current_position++)
    {
        int y = current_position / WIDTH;
        if (y >= HEIGHT)
            roll_up(&current_position);

        if (!string[i])
            break;

        int x = current_position % WIDTH;
        if (string[i] == '\n')
            current_position = current_position - x - 1 + WIDTH;
        else
            video[current_position] = string[i] | (color << 8);
    }
    __set_cursor_offset(current_position);
}

void putchar(char chr)
{
    short current_position = __get_cursor_offset();

    int x = current_position % WIDTH;
    if (chr == '\n')
        current_position = current_position - x - 1 + WIDTH;
    else
        video[current_position] = chr | (0b0111 << 8);

    current_position++;

    int y = current_position / WIDTH;
    if (y >= HEIGHT)
        roll_up(&current_position);

    __set_cursor_offset(current_position);
}

void print(char *string)
{
    print_color(string, 0b0111);
}

void KLOGE(char *string)
{
    print_color("[ERROR]", 0b0100);
    print(string);
}
void KLOGI(char *string)
{
    print_color("[OK]", 0b0010);
    print(string);
}
void KLOGW(char *string)
{
    print_color("[WARNING]", 0b0110);
    print(string);
}
void KLOGF(char *string)
{
    print_color("[FATAL]", 0b1100);
    print(string);
    while (true)
    {
    }
}
void print_hex(u32 num)
{
    char buffer[12];
    buffer[11] = '\0';
    buffer[10] = '0';
    int i = 10;
    if (num == 0)
        i--;
    for (; num > 0; i--)
    {
        u64 nn = num % 16;
        if (nn < 10)
            buffer[i] = nn + '0';
        else
            buffer[i] = nn - 0xa + 'A';
        num >>= 4;
    }
    print(buffer + i + 1);
}

void print_int(u32 num)
{
    char buffer[12];
    buffer[11] = '\0';
    buffer[10] = '0';
    int i = 10;
    if (num == 0)
        i--;
    for (; num > 0; i--)
    {
        buffer[i] = num % 10 + '0';
        num /= 10;
    }

    print(buffer + i + 1);
}

void hexdump(char *data, u32 size)
{
    u8 *ptr = (u8 *)data;
    const char *hex_digits = "0123456789ABCDEF";

    for (u32 i = 0; i < size; i += 16)
    {
        u32 chunk = size - i;
        if (chunk > 16)
            chunk = 16;

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
            if (j == 7)
                print(" ");
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
