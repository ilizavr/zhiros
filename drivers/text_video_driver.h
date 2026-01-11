#include "../types.h"
#include "../string.h"
#include "../ports.h"

#ifndef TEXTDRIVER

#define TEXTDRIVER

#define VGA_CTRL_REGISTER 0x3d4
#define VGA_DATA_REGISTER 0x3d5
#define VGA_OFFSET_LOW 0x0f
#define VGA_OFFSET_HIGH 0x0e

#define VGA_BLACK 0b0000
#define VGA_BLACK_LIGHT 0b1000
#define VGA_BLUE 0b0001
#define VGA_BLUE_LIGHT 0b1001
#define VGA_GREEN 0b0010
#define VGA_GREEN_LIGHT 0b1010
#define VGA_CYAN 0b0011
#define VGA_CYAN_LIGHT 0b1011
#define VGA_RED 0b0100
#define VGA_RED_LIGHT 0b1100
#define VGA_MAGENTA 0b0101
#define VGA_MAGENTA_LIGHT 0b1101
#define VGA_YELLOW 0b0110
#define VGA_YELLOW_LIGHT 0b1110
#define VGA_WHITE 0b0111
#define VGA_WHITE_LIGHT 0b1111

static U16 *video = (U16*)0xB8000;

const U8 WIDTH = 80, HEIGHT = 25;

void __set_cursor_offset(U16 offset){
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
	port_byte_out(VGA_DATA_REGISTER, offset&0xFF);
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
	port_byte_out(VGA_DATA_REGISTER, offset>>8);
}

U16 __get_cursor_offset() {
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
	U16 offset = port_byte_in(VGA_DATA_REGISTER);
	port_byte_out(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
	offset |= port_byte_in(VGA_DATA_REGISTER)<<8;
	return offset;
}

U16 __get_offset(U8 x, U8 y)
{
	return x+y*WIDTH;
}

U16 __get_color(U8 color, U8 background_color)
{
	return (color&0b1111) | ((background_color&0b1111)<<4);
}

void set_symbol(U8 x, U8 y, char sym, U8 color, U8 background_color)
{
	if(x>WIDTH||y>HEIGHT) return;

	video[__get_offset(x,y)] = sym | (__get_color(color,background_color)<<8);
}

void set_cursor_pos(U8 x, U8 y){
	if(x>WIDTH) return;
	if(y>HEIGHT) return;

	__set_cursor_offset(__get_offset(x,y));
}

struct vec2U8 get_cursor_pos()
{
	U16 off = __get_cursor_offset();
	struct vec2U8 ret;
	ret.x = off%WIDTH;
	ret.y = off/WIDTH;
	return ret;
}

#endif
