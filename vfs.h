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

void* read(char *path, int *size)
{
	struct disk*dsk = disks[current_diskid];

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

	void *buffer = kalloc(get_file_size(file,bpb));
        *(char*)buffer = 0;
        load_file(current_partstart,dsk,file,bpb,rootdir,buffer);

	if(size)*size = file->file_size;

	free(bpb);free(rootdir);free(root_buffer);free(file);

	return buffer;
}

void write(char* path,char* buffer, int size)
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

	if(buffer&&size)write_file(current_partstart,dsk,file,bpb,rootdir,buffer,size);

        writeRootDir(current_partstart,dsk,rootdir,bpb,root_buffer);
        free(bpb);free(rootdir);free(root_buffer);free(file);
}
