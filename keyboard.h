const char keyboard_map[128] =
{
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  '\t',                     /* Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
  '\n',                     /* Enter (Скан-код 0x1C) */
    0,                      /* Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    0,                      /* Left Shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   
    0,                      /* Right Shift */
  '*',
    0,                      /* Alt */
  ' ',                      /* Space */
    0,                      /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* F1-F10 keys */
    0,                      /* Num lock */
    0,                      /* Scroll lock */
    0,                      /* Home key */
    0,                      /* Up Arrow */
    0,                      /* Page Up */
  '-',
    0,                      /* Left Arrow */
    0,
    0,                      /* Right Arrow */
  '+',
    0,                      /* End key */
    0,                      /* Down Arrow */
    0,                      /* Page Down */
    0,                      /* Insert Key */
    0,                      /* Delete Key */
    0, 0, 0,
    0,                      /* F11 Key */
    0,                      /* F12 Key */
    0,                      
};

u8 last_pressed_key = 0;

u8 get_pressed_keycode()
{
	asm volatile("hlt");
	return last_pressed_key;
	
	//if((port_byte_in(0x64)&1) == 0)return 0;//key not pressed
	//return port_byte_in(0x60);
}

char getch()
{
	u8 key = 0;
	while(!key || key&0x80) key = get_pressed_keycode();

	char chr = keyboard_map[key];
	return chr;
}

void input(char * string, int maxlen)
{
	int i = 0;
	while(i<maxlen-1) {
		char chr = getch();
		if(chr==0) continue;
		if(chr == '\b') {
			if(i>0){
				__set_cursor_offset(__get_cursor_offset() - 1);
               			putchar(' ');
                		__set_cursor_offset(__get_cursor_offset() - 1);
			
				i--;
				string[i] = '\0';
			}
		}else if(chr=='\n'){
			putchar('\n');
			break;
		}else {
			string[i] = chr;
			putchar(chr);
			i++;
		}
	}
	string[i] = 0;
}
void keyboard_handler()
{
	last_pressed_key = port_byte_in(0x60);
	pic_eoi();
}

extern void keyboard_isr_handler();

void init_keyboard_interrupt()
{
	set_idt_gate(33, (u32)keyboard_isr_handler);
}
