#ifndef KEYBOARD
#define KEYBOARD

#include "../types.h"
#include "../ports.h"

U8 get_pressed_keycode()
{
	if(!port_byte_in(0x64))return 0;//кнопка не нажата
	return port_byte_in(0x60);
}

#endif
