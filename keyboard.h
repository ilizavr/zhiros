const char keyboard_map[128] =
{
    0,   0, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  ' ',                     /* Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
  '\n',                     /* Enter (Скан-код 0x1C) */
    0,                      /* Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    0,                      /* Left Shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   
    0,                      /* Right Shift */
  0,
    0,                      /* Alt */
  ' ',                      /* Space */
    0,                      /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* F1-F10 keys */
    0,                      /* Num lock */
    0,                      /* Scroll lock */
    0,                      /* Home key */
    24,                      /* Up Arrow */
    0,                      /* Page Up */
  '-',
    27, /* Left Arrow */                     
   0, 
    26,                      /* Right Arrow */
  '+',
    0,                      /* End key */
    25,                      /* Down Arrow */
    0,                      /* Page Down */
    0,                      /* Insert Key */
    0,                      /* Delete Key */
    0, 0, 0,
    0,                      /* F11 Key */
    0,                      /* F12 Key */
    0,                      
};
const char keyboard_map_shift[128] =
{
    0,   0, '!', '@', '#', '$', '%', '^', '&', '*',
  '(', ')', '_', '+', '\b', 
  ' ',
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 
  '\n',                    
    0,                      /* Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 
    0,                      /* Left Shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   
    0,                      /* Right Shift */
  0,
    0,                      /* Alt */
  ' ',                      /* Space */
    0,                      /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* F1-F10 keys */
    0,                      /* Num lock */
    0,                      /* Scroll lock */
    0,                      /* Home key */
    24,                      /* Up Arrow */
    0,                      /* Page Up */
  0,
    27, /* Left Arrow */                     
   0, 
    26,                      /* Right Arrow */
  0,
    0,                      /* End key */
    25,                      /* Down Arrow */
    0,                      /* Page Down */
    0,                      /* Insert Key */
    0,                      /* Delete Key */
    0, 0, 0,
    0,                      /* F11 Key */
    0,                      /* F12 Key */
    0,                      
};


u8 last_pressed_key = 0;
bool shift_pressed = 0;
bool ctrl_pressed = 0;

u8 get_pressed_keycode()
{
	asm volatile("hlt");
	u8 save = last_pressed_key;
	last_pressed_key = 0;
	return save;
}

char getch()
{
	u8 key = 0;
	while(!key || key&0x80) key = get_pressed_keycode();

	char chr = keyboard_map[key];
	if(shift_pressed) chr = keyboard_map_shift[key];
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
	if(last_pressed_key==0x2A)shift_pressed=true;
	if(last_pressed_key==0xAA)shift_pressed=false;
	if(last_pressed_key==0x1D)ctrl_pressed=true;
	if(last_pressed_key==0x9D)ctrl_pressed=false;	
	

	/*if(ctrl_pressed&&keyboard_map[last_pressed_key]=='c') {
		print("^C\n");
		asm volatile("cli");
		kill(0);
		create_process((u32)start_shell);
		last_pressed_key=0;
		asm volatile("sti");
	}*/	

	if(ctrl_pressed&&keyboard_map[last_pressed_key]>='0'&&keyboard_map[last_pressed_key]<='9') {
		clear_signal = true;
		current_process = keyboard_map[last_pressed_key]-'0';
		last_pressed_key = 0;
	}
	pic_eoi();
}

