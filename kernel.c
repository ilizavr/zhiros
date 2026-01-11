#include "drivers/text_video_driver.h"
#include "drivers/keyboard.h"
void main(){
	while(get_pressed_keycode()!=0x03){}//ждем пока не нажмут 1
	set_symbol(5,5,'9',VGA_BLUE,VGA_RED);
	set_cursor_pos(2,7);
}
