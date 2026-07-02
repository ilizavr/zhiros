#include "build/bios_asm_hex.h"

struct __attribute__((packed)) bios_asm_header_t
{
	u16 bios_lba;
	u16 reserved;
	struct dap_t *dap;
}* bios_asm_header;

struct __attribute__((packed)) dap_t
{
	u8 size;
	u8 res;
	u16 count;
	u16 offset;
	u16 segment;
	u32 lba;
	u32 lba2;

	u8 buffer[0];
};


void call_bios_lba(u32 op, u32 disk)
{
	((void(*)(u32,u32))bios_asm_header->bios_lba)(op,disk);
}

void bios_read_lba(u32 id, u32 lba, void* buffer, u32 blocks)
{
	asm volatile("cli");
	bios_asm_header->dap->size = 0x10;
	bios_asm_header->dap->res = 0;
	bios_asm_header->dap->count = blocks;
	bios_asm_header->dap->offset = (u16)bios_asm_header->dap->buffer;
	bios_asm_header->dap->segment = 0;
	bios_asm_header->dap->lba = lba;
	bios_asm_header->dap->lba2 = 0;

	call_bios_lba(0x42,id);
	memcpy(buffer,bios_asm_header->dap->buffer,blocks*512);
	

	asm volatile("sti");
	pic_eoi();
}

void bios_write_lba(u32 id, u32 lba, void* buffer, u32 blocks)
{
	asm volatile("cli");
	memcpy(bios_asm_header->dap->buffer,buffer,blocks*512);

	bios_asm_header->dap->size = 0x10;
	bios_asm_header->dap->res = 0;
	bios_asm_header->dap->count = blocks;
	bios_asm_header->dap->offset = (u16)bios_asm_header->dap->buffer;
	bios_asm_header->dap->segment = 0;
	bios_asm_header->dap->lba = lba;
	bios_asm_header->dap->lba2 = 0;

	call_bios_lba(0x43,id);
	asm volatile("sti");
	pic_eoi();
}
void init_bios()
{
	memcpy((void*)0x7000,build_bios,build_bios_len);
	bios_asm_header = (struct bios_asm_header_t*)0x7000;

	u8 disks_count = *(u8*)0x475;
	u8 added = 0;
	for(int i = 0;i<MAX_DISK_COUNT;i++)
	{
		if(disks[i]) continue;
		disks[i] = kalloc(sizeof(struct disk));
		disks[i]->name = "bios";
		disks[i]->id=0x80+added;
		disks[i]->lba_read=bios_read_lba;
		disks[i]->lba_write=bios_write_lba;
		added++;

		
		*(u16*)bios_asm_header->dap = 30;
		call_bios_lba(0x48,disks[i]->id);
		disks[i]->size=*(u32*)((u32)bios_asm_header->dap+0x10);
		
		read_mbr(disks[i]);

		if(added==disks_count)break;
	}
	KLOGI("BIOS detected ");
	print_int(disks_count);
	print(" hdd\n");
}


