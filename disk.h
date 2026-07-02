#define MAX_DISK_COUNT 16

struct mbr_part
{
	u8 boot_flag;
	u8 start_chs[3];
	u8 sys_id;
	u8 end_chs[3];
	u32 start_lba;
	u32 total_sectors;
};

struct disk
{
	char *name;
	u32 id;
	u32 size;
	void (*lba_read)(u32 id, u32 lba, void* buffer, u32 blocks);
	void (*lba_write)(u32 id, u32 lba, void* buffer, u32 blocks);

	struct mbr_part partitions[4];
};

struct __attribute__((packed)) fullmbr
{
	u8 boot[446];
	struct mbr_part partitions[4];
	u16 sign;
};

void read_mbr(struct disk*dsk)
{
	struct fullmbr *mbr = kalloc(512);
	dsk->lba_read(dsk->id,0,mbr,1);
	memcpy((char*)dsk->partitions,(char*)mbr->partitions,sizeof(struct mbr_part)*4);

	free(mbr);
}

struct disk* disks[MAX_DISK_COUNT] = {0};



