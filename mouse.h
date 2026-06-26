s32 mouse_x = 0;
s32 mouse_y = 0;
u8 mouse_left = 0;
u8 mouse_right = 0;

void mouse_wait(u8 type) {
	u32 timeout = 100000;
	if (type == 0) {
		while ((inb(0x64) & 2) && timeout--);
	} else {
		while (!(inb(0x64) & 1) && timeout--);
	}
}

void mouse_write(u8 data) {
	mouse_wait(0);
	outb(0x64, 0xD4);
	mouse_wait(0);
	outb(0x60, data);
}

u8 mouse_read() {
	while (1) {
		u8 status = inb(0x64);
		if ((status & 1) && (status & 0x20)) {
			return inb(0x60);
		}
	}
}

void mouse_init() {
	mouse_wait(0);
	outb(0x64, 0xA8);

	mouse_write(0xF4);
	mouse_read();
}

void process_mouse() {
    u8 cycle = 0;
    u8 packet[3];

    while (1) {
        packet[cycle] = mouse_read();

        if (cycle == 0 && !(packet[0] & 0x08)) {
            continue;
        }

        cycle++;

        if (cycle == 3) {
            cycle = 0;

            mouse_left = packet[0] & 0x01;
            mouse_right = packet[0] & 0x02;

            s16 dx = packet[1];
            s16 dy = packet[2];

            if (packet[0] & 0x10) dx |= 0xFF00;
            if (packet[0] & 0x20) dy |= 0xFF00;

            mouse_x += dx;
            mouse_y -= dy;
        }
    }
}
