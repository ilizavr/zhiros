#include "font8x16.h"


u8 colorscheme=0;
u64 fb_addr = 0;
u32 screen_width = 0;
u32 screen_height = 0;
u32 screen_pitch = 0;

u32 current_process = 1;

bool clear_signal = false;

const u32 vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

void put_pixel(u32 x, u32 y, u32 color) {
    u8* pixel_address = (u8*)(fb_addr + (y * screen_pitch) + (x * colorscheme));
    pixel_address[0] = color&0xFF;
    pixel_address[1] = (color>>8)&0xFF;
    pixel_address[2] = (color>>16)&0xFF;
    if(color==4) pixel_address[3] = 0xFF;
}

void put_sym(u8 sym, u32 startx, u32 starty,u32 color,u32 bgcolor)
{
	for(int y = 0;y<16;y++)
	{
		u8 font_row = font8x16[sym*16+y];
		for(int x = 0;x<8;x++)
		{
			if(font_row&0b10000000)put_pixel(startx+x,starty+y,color);
			else put_pixel(startx+x,starty+y,bgcolor);
			font_row<<=1;
		}
	}
}

void draw_horisontal_line(u32 startx, u32 endx, u32 y, u32 color)
{
	for(int x = startx; x<endx;x++)put_pixel(x,y,color);
}

void put_text(char *text, u32 startx, u32 starty, u32 color, u32 bgcolor)
{
	for(int i = 0;i<strlen(text);i++)
	{
		put_sym(text[i],startx+i*8,starty,color,bgcolor);
	}
}

void clearframe()
{
	for(int x = 0; x<screen_width;x++)
	for(int y = 0; y<screen_height;y++)
	put_pixel(x,y,0);
}
void fbdev_init(u64 addr, u32 width, u32 height, u32 pitch,u8 color) {
    colorscheme = color/8;
    fb_addr = addr; 
    screen_width = width; 
    screen_height = height; 
    screen_pitch = pitch;

    WIDTH = screen_width/8;
    HEIGHT = screen_height/16 - 1;
}

extern u32 timerticks;
u32 lastticks;

short old_video_buffer[100*100+0x40];
bool ega2fb_clear_signal = false;

void ega2fb() {
    if(!fb_addr)return;
    
    for (u32 row = 0; row < HEIGHT; row++) {
        for (u32 col = 0; col < WIDTH; col++) {
            u16 cell = video[row * WIDTH + col];
            u16 old = old_video_buffer[row*WIDTH+col];
            if(cell==old&&!ega2fb_clear_signal){
                continue;
            } else old_video_buffer[row*WIDTH+col]=cell;
            u8 symbol = cell & 0xFF;
            u8 attr = (cell >> 8) & 0xFF;

            u32 fg_color = vga_palette[attr & 0x0F];
            u32 bg_color = vga_palette[(attr >> 4) & 0x0F];
	    
            put_sym(symbol,col*8,row*16+17,fg_color,bg_color);
        }
    }
    
    ega2fb_clear_signal = false;

}

void windowsmanager()
{
	while(true)
	{
		if(clear_signal){
			clearframe();
			clear_signal = false;
		}
		if(tasks[current_process]){
			asm volatile("cli");

			char *name = tasks[current_process]->name;
			u32 x = (screen_width/8 - strlen(name)) * 4;
			put_text(name,x,0,0xFFFFFF,0);
			draw_horisontal_line(0,screen_width,16,0xFFFFFF);
			tasks[current_process]->drawframe();
  			asm volatile("sti");
		}
		else clearframe();
                asm volatile("hlt");
	}
}

struct image{
	u16 width;
	u16 height;
	u32 bytes[0];
};

void drawimage(struct image*img,int startx,int starty)
{
	for(int x = 0;x<img->width;x++)
		for(int y = 0;y<img->height;y++)
			put_pixel(x+startx,y+starty,img->bytes[y*img->width+x]);
}
