#include "font8x16.h"
#include "zhirglmath.h"

u8 colorscheme=0;
u64 fb_addr = 0;
u32 screen_width = 0;
u32 screen_height = 0;
u32 screen_pitch = 0;
u32 framebuffer_size = 0;
u8 *back_frame = 0;

u32 speed = 0;

u32 current_process = 1;

bool clear_signal = false;

typedef struct {
    int x;
    int y;
} Point;

const u32 vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static inline __attribute__((always_inline, optimize("O3")))
void present_frame() {
    for (u32 y = 0; y < screen_height; y++)
        memcpy((void*)(fb_addr + y * screen_pitch), back_frame + y * screen_pitch, screen_pitch);
}

static inline __attribute__((always_inline, optimize("O3")))
void put_pixel(u32 x, u32 y, u32 color) {
    u32 *pixel = (u32*)(back_frame + y * screen_pitch + x * colorscheme);

    *pixel &= 0xFF000000;
    *pixel |= color;
}

void put_sym(u8 sym, u32 startx, u32 starty,u32 color,u32 bgcolor)
{
	for(int y = 0;y<16;y++)
	{
		u8 font_row = font8x16[sym*16+y];
		for(int x = 0;x<8;x++)
		{
			if(font_row&0b10000000) 
                put_pixel(startx+x,starty+y,color);
			else 
                put_pixel(startx+x,starty+y,bgcolor);

			font_row<<=1;
		}
	}
}

void draw_horisontal_line(u32 startx, u32 endx, u32 y, u32 color)
{
	for(int x = startx; x < endx;x++) put_pixel(x,y,color);
}

void draw_vertices(Point *point_start, int size, int color) {

	for (int i = 0; i < size; i++) {
		int startx = point_start[i].x;
		int starty = point_start[i].y;
		int endx = point_start[(i+1) % size].x;
		int endy = point_start[(i+1) % size].y;

		int dx = abs_val(endx - startx);
		int dy = abs_val(endy - starty);
			
		int sx = (startx < endx) ? 1 : -1;
		int sy = (starty < endy) ? 1 : -1;
			
		int err = dx - dy;
		int e2;

		while (1) {
			put_pixel(startx, starty, color);
				
			if (startx == endx && starty == endy) {
				break;
			}
				
			e2 = 2 * err;
				
			if (e2 > -dy) {
				err -= dy;
				startx += sx;
			}
				
			if (e2 < dx) {
				err += dx;
				starty += sy;
			}
		}
	}
}

void fill_polygon(Point *points, int count, int color)
{

    if (points == 0 || count < 3)
        return;

    int min_y = points[0].y;
    int max_y = points[0].y;

    for (int i = 1; i < count; i++)
    {
        if (points[i].y < min_y)
            min_y = points[i].y;

        if (points[i].y > max_y)
            max_y = points[i].y;
    }

    int intersections[count];

    for (int y = min_y; y <= max_y; y++)
    {
        int intersection_count = 0;

        for (int i = 0; i < count; i++)
        {
            int next = (i + 1) % count;

            int x1 = points[i].x;
            int y1 = points[i].y;

            int x2 = points[next].x;
            int y2 = points[next].y;

            if ((y1 <= y && y < y2) || (y2 <= y && y < y1))
            {
                int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);

                intersections[intersection_count] = x;
                intersection_count++;
            }
        }

        for (int i = 0; i < intersection_count - 1; i++)
        {
            for (int j = i + 1; j < intersection_count; j++)
            {
                if (intersections[i] > intersections[j])
                {
                    int temp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        for (int i = 0; i < intersection_count - 1; i++)
        {
            int start_x = intersections[i];
            int end_x = intersections[i + 1];

            for (int x = start_x; x <= end_x; x++)
            {
                put_pixel(x, y, color);
            }
        }
    }
}

void put_text(char *text, u32 startx, u32 starty, u32 color, u32 bgcolor)
{
	int len = strlen(text);
	for(int i = 0;i<len;i++)
	{
		put_sym(text[i],startx+i*8,starty,color,bgcolor);
	}
}

static inline __attribute__((always_inline, optimize("O2")))
void clearframe() {
    memset(back_frame, 0, framebuffer_size);
}

void fbdev_init(u64 addr, u32 width, u32 height, u32 pitch,u8 color) {
    colorscheme = color/8;
    fb_addr = addr; 
    screen_width = width; 
    screen_height = height; 
    screen_pitch = pitch;

    framebuffer_size = screen_pitch * screen_height;
    back_frame = kalloc(framebuffer_size);

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

            if(cell==old && !ega2fb_clear_signal){
                continue;
            } else 
                old_video_buffer[row*WIDTH+col]=cell;

            u8 symbol = cell & 0xFF;
            u8 attr = (cell >> 8) & 0xFF;

            u32 fg_color = vga_palette[attr & 0x0F];
            u32 bg_color = vga_palette[(attr >> 4) & 0x0F];
	    
            put_sym(symbol,col*8,row*16+17,fg_color,bg_color);
        }
    }
    
    ega2fb_clear_signal = false;

}

static inline __attribute__((always_inline, optimize("O3")))
bool disable_sch = false;
void windowsmanager()
{
        int old_time = ticks;

        while(true)
        {
                if(clear_signal){
                        clearframe();
                        clear_signal = false;
                }
                if(tasks[current_process]){
                        disable_sch=true;

                        char *name = tasks[current_process]->name;
                        u32 x = (screen_width/8 - strlen(name)) * 4;

                        put_text(name,x,0,0xFFFFFF,0);
                        draw_horisontal_line(0,screen_width,16,0xFFFFFF);

                        tasks[current_process]->drawframe();
                        //pic_eoi();

                        if(ticks > old_time){
                                int fps = 1000/(ticks-old_time);
                                char buffer[24];
                                char*fps_text = int2str(fps,buffer);
                                put_text("   ",screen_width-3*4-20,0,0xFFFFFF,0);
                                put_text(fps_text,screen_width-strlen(fps_text)*4-20,0,0xFFFFFF,0);
                        }
                        old_time = ticks;

                        present_frame();

                        disable_sch=false;
                }
                else 
                    clearframe();
                    present_frame();
                // asm volatile("hlt");
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

vec2 figure_center(Point *points, int count)
{
    int min_x = points[0].x;
    int max_x = points[0].x;
    int min_y = points[0].y;
    int max_y = points[0].y;

    for (int i = 1; i < count; i++) {
        if (points[i].x < min_x) min_x = points[i].x;
        if (points[i].x > max_x) max_x = points[i].x;
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }

    return vec2_create(
        (min_x + max_x) / 2.0f,
        (min_y + max_y) / 2.0f
    );
}

void vec2_rotate(Point *points, int size, vec2 center, float time, float angle) {
    for (int i = 0; i < size; i++){
        float radians = (angle * PI / 180.0f)*time;

        float x = points[i].x - center.x;
        float y = points[i].y - center.y;

        points[i].x = center.x + x * cos(radians) - y * sin(radians);
        points[i].y = center.y + x * sin(radians) + y * cos(radians);
    }
}

void vec2_locate(Point *points, int size, int x, int y, u32 speed) {
    vec2 center = figure_center(points, size);

    int dx = x - center.x;
    int dy = y - center.y;
    for (int i = 0; i < size; i++) {
        points[i].x += dx + speed;
        points[i].y += dy + speed;
    }
}

#include "zhirGL.h"