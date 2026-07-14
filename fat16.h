#define LONG_NAME (1 << 30)

typedef struct
{
    u8 jump[3];
    u8 oem[8];
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sectors;
    u8 number_of_fats;
    u16 root_entries;
    u16 total_sectors_16;
    u8 media_descriptor;
    u16 sectors_per_fat;
    u16 sectors_per_track;
    u16 number_of_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
    u8 drive_number;
    u8 reserved;
    u8 boot_signature;
    u32 volume_id;
    u8 volume_label[11];
    u8 fs_type[8];
    u8 boot_code[448];
    u16 signature; // 0x55AA
} __attribute__((packed)) BPB;

typedef struct
{
    u32 fat_start;
    u32 root_start;
    u32 root_size;
    u32 data_start;
} __attribute__((packed)) RootDir;

typedef struct
{
    u8 name[11];
    u8 attr; //0x20 for a file,0x10 for a directory
    u8 reserved[14];
    u16 first_cluster;
    u32 file_size;
} __attribute__((packed)) DirEntry;

char upchr(char chr)
{
    if (chr >= 'a' && chr <= 'z')
        return chr - 32;
    return chr;
}
char *convert_filename(const char *file) 
{
    char *ret = (char *)kalloc(11);
    if (!ret) return 0;

    for (int i = 0; i < 11; i++) 
    {
        ret[i] = ' ';
    }

    int i = 0;
    while (file[i] != '\0' && file[i] != '.' && i < 8) 
    {
        ret[i] = upchr(file[i]);
        i++;
    }

    while (file[i] != '\0' && file[i] != '.') 
    {
        i++;
    }

    if (file[i] == '.') 
    {
        i++;
        int ext_len = 0;
        
        while (file[i] != '\0' && ext_len < 3) 
        {
            ret[8 + ext_len] = upchr(file[i]);
            i++;
            ext_len++;
        }
    }

    return ret;
}

char* convert_dirname(const char* file)
{
char* ret = (char*)kalloc(11);
if(!ret) return 0;
for(int i=0;i<11;i++)
{
ret[i] = ' ';
}
int i=0;
while(file[i] != '\0')
{
ret[i] = upchr(file[i]);
++i;
}

return ret;
}

BPB *read_first_sector(u32 partstart,struct disk *dsk)
{
    u8 *buf = kalloc(512);
    dsk->lba_read(dsk->id, 0+partstart, buf, 1);

    if (buf[0x1FE] != 0x55 && buf[0x1FF] != 0xAA)
    {
        KLOGE("invalid disk. Couldn't load FAT16");
        return 0;
    }

    return (BPB *)buf;
}

RootDir *calculateRootDir(BPB *bpb)
{
    RootDir *root = kalloc(sizeof(RootDir));

    u32 fat_start = bpb->reserved_sectors;
    u32 root_start = fat_start + bpb->number_of_fats * bpb->sectors_per_fat;
    u32 root_size = bpb->root_entries * 32 / bpb->bytes_per_sector;
    u32 data_start = root_start + root_size;

    root->fat_start = fat_start;
    root->root_start = root_start;
    root->root_size = root_size;
    root->data_start = data_start;

    return root;
}

u8 *readRootDir(u32 partstart,struct disk *dsk, RootDir *root, BPB *bpb)
{
    u8 *root_buffer = kalloc(root->root_size * bpb->bytes_per_sector);

    for (u32 i = 0; i < root->root_size; i++)
    {
        dsk->lba_read(dsk->id, partstart+root->root_start + i, root_buffer + (i * bpb->bytes_per_sector), 1);
    }

    return root_buffer;
}

void print_dir(u8 *root_buffer, BPB* bpb)
{
    u16 entries = bpb->root_entries;
    DirEntry *entry = (DirEntry *)root_buffer;

    for (u16 i = 0; i < entries; i++)
    {
	u8 c = (entry + i)->name[0];

	if (c == 0x00)
	    break;
	if (c == 0xE5)
	    continue;
	if ((entry+i)->attr == 0x0f) continue;
	if((entry+i)->attr==0x20)
	{
	print((entry+i)->name);
	print("\n");
	}else if((entry+i)->attr=0x10)
	{
        print_color((entry+i)->name,0b0011);
	print("\n");
	}

    }

}
DirEntry *find_file(u8 *root_buffer, BPB *bpb, char *name)
{
     if (strlen(name) > 11)
        return 0;

name = convert_filename(name);
    u16 entries = bpb->root_entries;
    DirEntry *entry = (DirEntry *)root_buffer;

    for (u16 i = 0; i < entries; i++)
    {
        u8 c = (entry + i)->name[0];

        if (c == 0x00)
            break;
        else if (c == 0xE5)
            continue;

        if (memcmp((entry + i)->name, name, 11) == 0){
            free(name);
		return (entry + i);
	}
    }

free(name);
    return 0;
}

u32 get_file_size(DirEntry *file, BPB *bpb)
{
    int bytes_per_cluster = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    int ret = (file->file_size / bytes_per_cluster + 1) * bytes_per_cluster;
    return ret;
}

void load_file(u32 partstart,struct disk *dsk, DirEntry *file, BPB *bpb, RootDir *root, u8 *dest)
{
   u32 cluster = file->first_cluster;
    u32 file_offset = 0;

    while (cluster < 0xFFF8 && file_offset < file->file_size)
    {
        u32 lba = root->data_start + (cluster - 2) * bpb->sectors_per_cluster;

        for (u32 i = 0; i < bpb->sectors_per_cluster; i++)
        {
            dsk->lba_read(dsk->id, partstart+lba + i, dest + file_offset, 1);
            file_offset += bpb->bytes_per_sector;
        }

        u32 fat_offset = cluster * 2;
        u8 *fat_sector = kalloc(512);

        dsk->lba_read(dsk->id, partstart+ root->fat_start + fat_offset / 512, fat_sector, 1);
        cluster = *(u16 *)(fat_sector + fat_offset % 512);

        free(fat_sector);
    }
}

u32 find_free_cluster(u32 partstart,struct disk *dsk, BPB *bpb, RootDir *root)
{
    u8 fat_sector[512];
    u32 total_clusters = (bpb->total_sectors_32 - root->data_start) / bpb->sectors_per_cluster;

    for (u32 cluster = 2; cluster < total_clusters; cluster++)
    {
        u32 fat_offset = cluster * 2;
        u32 fat_sector_index = fat_offset / 512;
        u32 fat_offset_in_sector = fat_offset % 512;

        dsk->lba_read(dsk->id, partstart+root->fat_start + fat_sector_index, fat_sector, 1);

        u16 value = *(u16 *)(fat_sector + fat_offset_in_sector);

        if (value == 0x0000)
        {
            return cluster;
        }
    }

    return 0;
}

u16 read_fat(u32 partstart,struct disk *dsk, u32 cluster, BPB *bpb, RootDir *root)
{
    u32 fat_offset = cluster * 2;
    u32 fat_sector_idx = fat_offset / bpb->bytes_per_sector;
    u32 fat_offset_in_sector = fat_offset % bpb->bytes_per_sector;

    u8 fat_sector[512];
    dsk->lba_read(dsk->id, partstart+root->fat_start + fat_sector_idx, fat_sector, 1);

    return *(u16 *)(fat_sector + fat_offset_in_sector);
}

void write_fat(u32 partstart,struct disk *dsk, u32 cluster, u16 next, BPB *bpb, RootDir *root)
{
    u32 fat_offset = cluster * 2;
    u32 fat_sector_idx = fat_offset / bpb->bytes_per_sector;
    u32 fat_offset_in_sector = fat_offset % bpb->bytes_per_sector;

    u8 fat_sector[512];
    dsk->lba_read(dsk->id,partstart+root->fat_start + fat_sector_idx, fat_sector, 1);

    *(u16 *)(fat_sector + fat_offset_in_sector) = next;

    dsk->lba_write(dsk->id,partstart+root->fat_start + fat_sector_idx, fat_sector, 1);

    if (bpb->number_of_fats == 2)
    {
        dsk->lba_write(dsk->id,partstart+root->fat_start + bpb->sectors_per_fat + fat_sector_idx, fat_sector, 1);
    }
}

DirEntry *create_file(u8 *root_buffer, BPB *bpb, char *name,u8 attr)
{
    if (strlen(name) > 11)
        return (DirEntry*)LONG_NAME;

if(attr==0x20) name=convert_filename(name);
else if(attr==0x10) name=convert_dirname(name);

    u16 entries = bpb->root_entries;

    DirEntry *entry = (DirEntry *)root_buffer;

    for (int i = 0; i < entries; i++)
    {
        u8 c = (entry + i)->name[0];

        if (c == 0x00 || c == 0xE5)
        {
            memcpy((entry + i)->name, name, 11);
            (entry + i)->attr = attr;
            (entry + i)->first_cluster = 0x00;
            (entry + i)->file_size = 0x00;
	    free(name);
            return (entry + i);
        }
    }
	free(name);
    return 0;
}

int write_file(u32 partstart,struct disk *dsk, DirEntry *file, BPB *bpb, RootDir *root, u8 *data, u32 size)
{
    if (file->first_cluster == 0)
    {
        u32 cluster = find_free_cluster(partstart,dsk, bpb, root);
        if (cluster == 0)
            return -1;
        file->first_cluster = cluster;
    }

    u32 cluster = file->first_cluster;
    u32 written = 0;

    while (written < size)
    {
        u32 lba = root->data_start + (cluster - 2) * bpb->sectors_per_cluster;
        for (u32 i = 0; i < bpb->sectors_per_cluster && written < size; i++)
        {
            dsk->lba_write(dsk->id,partstart+lba + i, data + written, 1);
            written += bpb->bytes_per_sector;
        }

        u16 next = read_fat(partstart,dsk, cluster, bpb, root);
        if (next >= 0xFFF8)
        {
            next = find_free_cluster(partstart,dsk, bpb, root);
            if (next == 0)
                return -1;
            write_fat(partstart,dsk, cluster, next, bpb, root);
        }
        cluster = next;
    }

    file->file_size = size;
    return 0;
}

void writeRootDir(u32 partstart,struct disk *dsk, RootDir *root, BPB *bpb, u8 *root_buffer)
{
    for (u32 i = 0; i < root->root_size; i++)
    {
        dsk->lba_write(dsk->id, partstart+root->root_start + i, root_buffer + (i * bpb->bytes_per_sector), 1);
    }
}
