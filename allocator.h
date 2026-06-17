extern char _kernel_end[];
extern char _kernel_start[];
#define alligment(size) (((size) + ((0x20) - 1)) & ~((0x20) - 1))
struct memoryblock
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

struct memoryblock headblock;


void *kalloc(u32 size)
{
	size=alligment(size);
	struct memoryblock *current = headblock.next;

	while(current)
	{
		if(current->size >= size && !current->allocated)
		{
			if((current->size - size) > 64)
			{
				struct memoryblock* newblock = (struct memoryblock*)((u64)current+sizeof(struct memoryblock)+size);
				newblock->next = current->next;
				current->next=newblock;
				if(newblock->next) newblock->next->prev = newblock;
				newblock->prev=current;
				newblock->size=current->size-size-sizeof(struct memoryblock);
				current->size=size;
				newblock->allocated = 0;
			}
			current->allocated = 1;
			return (void*)((u64)current+sizeof(struct memoryblock));
		}
		current = current->next;
	}

	KLOGE("free memory not found\n");

	return 0;
	
}
char * strdup(char *s)
{
	char * ns = kalloc(strlen(s)+1);
	strcpy(ns,s);
	return ns;
}
void free(void *addr)
{
	struct memoryblock *block = (struct memoryblock*)((u64)addr - sizeof(struct memoryblock));
	block->allocated = 0;//TODO: ADD defragmentation
}

void init_alloc_multiboot(u32 mmap_addr, u32 mmap_len)
{
	struct memoryblock *current = &headblock;

	u32 available_memory = 0;	

	struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mmap_addr;
	u32 mmap_end = mmap_addr + mmap_len;
	
	while((u32)mmap<mmap_end)
	{
		if(mmap->type == 1) //available
		{
			u64 mapend = mmap->addr+mmap->len;
			if(mmap->addr<_kernel_end)
			{ 
				KLOGW("multiboot marked kernel loading space as available. \n");
				if(mapend>_kernel_end) {
					mmap->addr = alligment((u64)_kernel_end);
					mmap->len=mapend-mmap->addr;
				}else{
					KLOGW("skip region\n");
					mmap = (struct multiboot_mmap_entry*)((u32)mmap+mmap->size+4);
					continue;
				}
			}
			if(mapend>=0x100000000) {
				 mmap = (struct multiboot_mmap_entry*)((u32)mmap+mmap->size+4);
                                 continue;
			} 
			if(mmap->len>1024){
				current->next = (struct memoryblock*)mmap->addr;
				current->next->prev = current;
				current->next->size = mmap->len - sizeof(struct memoryblock);
				current->next->allocated = 0;
				current->next->next = 0;	
				current = current->next;		

				available_memory+=mmap->len;
			}
		}
		mmap = (struct multiboot_mmap_entry*)((u32)mmap+mmap->size+4);
	}
	available_memory>>=20;

	if(available_memory<256) KLOGE("RAM < 256M\n");	

	KLOGI("allocator inited. free memory ");
	print_int(available_memory);
	print("M\n");
}
