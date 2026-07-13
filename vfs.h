#define STUB_FD 1040

u32 current_diskid = 0;
u32 current_partstart = 0;

void seldisk(char *code)
{
        current_diskid = code[0]-'a';
        if(current_diskid>MAX_DISK_COUNT||disks[current_diskid]==0){
                current_diskid = 0;
                current_partstart = 0;
                KLOGE("invalid disk code");
                return;
        }
        if(code[1]<'0'||code[1]>'3'){
                current_partstart = 0;
                return;
        }
        u8 partcode = code[1]-'0';
        struct disk*dsk = disks[current_diskid];
        current_partstart = dsk->partitions[partcode].start_lba;

}

void write_buf(char* buf,char* data,int size) {
for(int i = 0;i<size;i++){
buf[i] = data[i];
}

}

char* read_buf(char* buf,int size) {
char* new_b = (char*)kalloc(sizeof(char) * size);

for(int i = 0;i<size;i++){
new_b[i] = buf[i];
}
new_b[size] = '\0';

return new_b;
}

u64 read(int fd, void* buf, u64 count)
{
	struct disk*dsk = disks[current_diskid];
        char* path = read_buf((char*)buf,11);

	BPB* bpb = read_first_sector(current_partstart,dsk);
        RootDir* rootdir=calculateRootDir(bpb);
        u8* root_buffer = readRootDir(current_partstart,dsk,rootdir,bpb);

        DirEntry *file = find_file(root_buffer,bpb,path);

	if(!file)
        {
                KLOGE("file not found\n");
                free(bpb);free(rootdir);free(root_buffer);

		return 0;
        }

        *(char*)buf = 0;
        
	load_file(current_partstart,dsk,file,bpb,rootdir,buf);

	count = file->file_size;

	free(bpb);free(rootdir);free(root_buffer);free(file);

	return 1;
}

//there's fd parameter is a stub to compatiable POSIX standard
//the file name stores in first 11 bytes of buffer buf Since there's no file descriptor
//system to generate these ones based on name. That's temporarly
void write(int fd,char* path,const void*  buf, u64 count)
{
        struct disk *dsk =disks[current_diskid];
        BPB* bpb = read_first_sector(current_partstart,dsk);
        RootDir* rootdir=calculateRootDir(bpb);
        u8* root_buffer = readRootDir(current_partstart,dsk,rootdir,bpb);
        DirEntry *file = find_file(root_buffer,bpb,path);
        if(!file)
        {

        file = create_file(root_buffer,bpb,path);
        if(!file && file != LONG_NAME)
        {
                KLOGE("file not found and cant create of it\n");

                free(bpb);free(rootdir);free(root_buffer); free(file);

                return;
        } else if(file == LONG_NAME)
        {
         KLOGE("the file name is too long");

        free(bpb);free(rootdir);free(root_buffer); free(file);
        return;
        }

        }

	if(buf&&count)write_file(current_partstart,dsk,file,bpb,rootdir,buf,count);

        writeRootDir(current_partstart,dsk,rootdir,bpb,root_buffer);
        free(bpb);free(rootdir);free(root_buffer);free(file);
}
