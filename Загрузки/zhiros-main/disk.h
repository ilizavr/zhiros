struct disk
{
    char *name;
    u32 id;
    u32 size;
    void (*lba_read)(u32 id, u32 lba, void *buffer, u32 blocks);
    void (*lba_write)(u32 id, u32 lba, void *buffer, u32 blocks);
};

#define MAX_DISK_COUNT 4

struct disk *disks[MAX_DISK_COUNT] = {0};
