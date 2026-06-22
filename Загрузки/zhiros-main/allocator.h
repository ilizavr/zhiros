extern char _kernel_end[];
extern char _kernel_start[];
#define alignment(size) (((size) + ((0x10) - 1)) & ~((0x10) - 1))
__attribute__((packed)) struct memoryblock
{
    u32 size;
    char allocated;

    struct memoryblock *prev;
    struct memoryblock *next;
};
__attribute__((packed)) struct multiboot_mmap_entry
{
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
};

struct memoryblock headblock = {0, 1, 0, 0};

void *kalloc(u32 size)
{
    size = alignment(size);
    struct memoryblock *current = headblock.next;

    while (current)
    {
        if (current->size >= size && !current->allocated)
        {
            if ((current->size - size) > 64)
            {
                struct memoryblock *newblock = (struct memoryblock *)((u64)current + sizeof(struct memoryblock) + size);
                newblock->next = current->next;
                current->next = newblock;
                if (newblock->next)
                    newblock->next->prev = newblock;
                newblock->prev = current;
                newblock->size = current->size - size - sizeof(struct memoryblock);
                current->size = size;
                newblock->allocated = 0;
            }
            current->allocated = 1;
            return (void *)((u64)current + sizeof(struct memoryblock));
        }
        current = current->next;
    }

    KLOGE("free memory not found\n");

    return 0;
}
char *strdup(char *s)
{
    char *ns = kalloc(strlen(s) + 1);
    strcpy(ns, s);
    return ns;
}
void free(void *addr)
{
    if (!addr)
        return;
    struct memoryblock *block = (struct memoryblock *)((u64)addr - sizeof(struct memoryblock));
    block->allocated = 0;

    if (block->next->allocated == 0)
    {
        block->size += block->next->size + sizeof(struct memoryblock);
        block->next->next->prev = block;
        block->next = block->next->next;
    }
    if (block->prev->allocated == 0)
    {
        block = block->prev;

        block->size += block->next->size + sizeof(struct memoryblock);
        block->next->next->prev = block;
        block->next = block->next->next;
    }
}

void init_alloc_multiboot(u32 mmap_addr, u32 mmap_len, u32 ramdisk_end)
{
    struct memoryblock *current = &headblock;

    u32 available_memory = 0;

    struct multiboot_mmap_entry *mmap = (struct multiboot_mmap_entry *)mmap_addr;
    u32 mmap_end = mmap_addr + mmap_len;

    while ((u32)mmap < mmap_end)
    {
        if (mmap->type == 1) // available
        {
            u64 mapend = mmap->addr + mmap->len;
            if (mmap->addr < ramdisk_end)
            {
                // KLOGW("multiboot marked kernel/ramdisk loading space as available. \n");
                if (mapend > ramdisk_end)
                {
                    mmap->addr = alignment((u64)ramdisk_end);
                    mmap->len = mapend - mmap->addr;
                }
                else
                {
                    // KLOGW("skip region\n");
                    mmap = (struct multiboot_mmap_entry *)((u32)mmap + mmap->size + 4);
                    continue;
                }
            }
            if (mapend >= 0x100000000)
            {
                mmap = (struct multiboot_mmap_entry *)((u32)mmap + mmap->size + 4);
                continue;
            }
            if (mmap->len > 1024)
            {
                current->next = (struct memoryblock *)mmap->addr;
                current->next->prev = current;
                current->next->size = mmap->len - sizeof(struct memoryblock);
                current->next->allocated = 0;
                current->next->next = 0;
                current = current->next;

                available_memory += mmap->len;
            }
        }
        mmap = (struct multiboot_mmap_entry *)((u32)mmap + mmap->size + 4);
    }
    available_memory >>= 20;

    if (available_memory < 256)
        KLOGE("RAM < 256M\n");

    KLOGI("allocator inited. free memory ");
    print_int(available_memory);
    print("M\n");
}
