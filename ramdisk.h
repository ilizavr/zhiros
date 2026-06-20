u32 ramdisk_size = 0;

char *ramdisk_buffers[MAX_DISK_COUNT] = {0};

void ramdisk_read(u32 id, u32 lba, void * buffer, u32 blocks)
{
	memcpy(buffer,ramdisk_buffers[id]+lba*512,blocks*512);
}
void ramdisk_write(u32 id, u32 lba, void * buffer, u32 blocks)
{
        memcpy(ramdisk_buffers[id]+lba*512,buffer,blocks*512);
}

void add_ram_disk(char *name, char* buffer,u32 size)
{
	struct disk* newdisk = kalloc(sizeof(struct disk));
	newdisk->lba_read = ramdisk_read;
	newdisk->lba_write = ramdisk_write;
	newdisk->size = size;

	bool diskadded = false;

	for(int i = 0;i<MAX_DISK_COUNT;i++)
	{
		if(disks[i])continue;
		newdisk->id = i;
		ramdisk_buffers[i] = buffer;
		disks[i] = newdisk;
		diskadded = true;
		break;
	}
	if(!diskadded){
		KLOGE("cannot add new disk\n");
		free(newdisk);
		return;
	}

	newdisk->name = strdup(name);
	
	KLOGI("add ramdisk name:");
	print(name);
	print(" size:");
	print_int(size>>20);
	print("MB\n");
}

