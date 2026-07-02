
struct ide_device
{
	u16 base;
	u16 control;
	u8 drive;
};

struct ide_device ide_drives[MAX_DISK_COUNT];

void ide_delay(u16 control)
{
	port_byte_in(control);
	port_byte_in(control);
	port_byte_in(control);
	port_byte_in(control);
}

u8 ide_poll(u16 base, bool check_drq)
{
	for(int i = 0; i < 100000; i++)
	{
		u8 status = port_byte_in(base + 7);
		if(!(status & 0x80))
		{
			if(!check_drq || (status & 0x08)) return 1;
		}
	}
	return 0;
}

void ide_read_sectors(u32 id, u32 lba, void* buffer, u32 blocks)
{
	u16 base = ide_drives[id].base;
	u16 control = ide_drives[id].control;
	u8 drive = ide_drives[id].drive;
	u16* ptr = (u16*)buffer;

	for(u32 i = 0; i < blocks; i++)
	{
		u32 current_lba = lba + i;
		port_byte_out(base + 6, (drive == 0 ? 0xE0 : 0xF0) | ((current_lba >> 24) & 0x0F));
		port_byte_out(base + 1, 0);
		port_byte_out(base + 2, 1);
		port_byte_out(base + 3, current_lba & 0xFF);
		port_byte_out(base + 4, (current_lba >> 8) & 0xFF);
		port_byte_out(base + 5, (current_lba >> 16) & 0xFF);
		port_byte_out(base + 7, 0x20);

		ide_delay(control);

		if(!ide_poll(base, true)) return;

		for(int j = 0; j < 256; j++)
		{
			*ptr = port_word_in(base);
			ptr++;
		}
	}
}

void ide_write_sectors(u32 id, u32 lba, void* buffer, u32 blocks)
{
	u16 base = ide_drives[id].base;
	u16 control = ide_drives[id].control;
	u8 drive = ide_drives[id].drive;
	u16* ptr = (u16*)buffer;

	for(u32 i = 0; i < blocks; i++)
	{
		u32 current_lba = lba + i;
		port_byte_out(base + 6, (drive == 0 ? 0xE0 : 0xF0) | ((current_lba >> 24) & 0x0F));
		port_byte_out(base + 1, 0);
		port_byte_out(base + 2, 1);
		port_byte_out(base + 3, current_lba & 0xFF);
		port_byte_out(base + 4, (current_lba >> 8) & 0xFF);
		port_byte_out(base + 5, (current_lba >> 16) & 0xFF);
		port_byte_out(base + 7, 0x30);

		ide_delay(control);

		if(!ide_poll(base, true)) return;

		for(int j = 0; j < 256; j++)
		{
			port_word_out(base, *ptr);
			ptr++;
		}

		port_byte_out(base + 7, 0xE7);
		ide_delay(control);
		ide_poll(base, false);
	}
}

void ide_add_disk(u16 base, u16 control)
{
	for(u8 drive = 0; drive < 2; drive++)
	{
		port_byte_out(base + 6, drive == 0 ? 0xA0 : 0xB0);
		ide_delay(control);

		port_byte_out(base + 2, 0);
		port_byte_out(base + 3, 0);
		port_byte_out(base + 4, 0);
		port_byte_out(base + 5, 0);
		port_byte_out(base + 7, 0xEC);

		ide_delay(control);

		u8 status = port_byte_in(base + 7);
		if(status == 0) continue;

		if(!ide_poll(base, true)) continue;

		u16 info[256];
		for(int i = 0; i < 256; i++)
		{
			info[i] = port_word_in(base);
		}

		u32 sectors = *((u32*)(info + 60));
		if(sectors == 0) continue;

		int idx = -1;
		for(int i = 0; i < MAX_DISK_COUNT; i++)
		{
			if(!disks[i])
			{
				idx = i;
				break;
			}
		}
		if(idx == -1) return;

		ide_drives[idx].base = base;
		ide_drives[idx].control = control;
		ide_drives[idx].drive = drive;

		struct disk* d = kalloc(sizeof(struct disk));
		char model[41];
		for(int i = 0; i < 20; i++)
		{
			u16 w = info[27 + i];
			model[i * 2] = (char)(w >> 8);
			model[i * 2 + 1] = (char)(w & 0xFF);
		}
		model[40] = 0;

		int len = 39;
		while(len >= 0 && (model[len] == ' ' || model[len] == 0))
		{
			model[len] = 0;
			len--;
		}
		d->name = strdup(model);
		d->id = idx;
		d->size = sectors;
		d->lba_read = ide_read_sectors;
		d->lba_write = ide_write_sectors;

		disks[idx] = d;

		read_mbr(d);	

		KLOGI("IDE disk connected: ");
		print(model);
		print("\n");
	}
}

