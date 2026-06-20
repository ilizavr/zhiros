#include "int_types.h"
#include "ports.h"
#include "print.h"
#include "allocator.h"
#include "keyboard.h"
#include "function_manager.h"
#include "shell.h"
#include "cpu.h"
#include "time.h"
#include "disk.h"
#include "ramdisk.h"
#include "neuro_ide.h"
#include "pci.h"
#include "fat16.h"


struct object *echo(struct objectArray* args)
{
	for(int i = 0; i<args->count;i++)
	{
		if(args->objs[i].type==OBJECT_STRING){
			print(args->objs[i].data);
			print(" ");
		}
	}
	print("\n");
	return 0;
}

struct object *clear(struct objectArray* args)
{
	cls();
	return 0;
}

char * fetch_logo[]={
	"         _.--._",
	"    _.-.'      `.-._",
	"  .' ./`--...--'\\   `.",
	"  `.'.`--.._..--'   .'",
	"    `-..__    __..-'",
	"          ````",
};

struct object *screenfetch(struct objectArray* args)
{
	u32 used=0, freemem=0;
	struct memoryblock *current = headblock.next;
	while(current)
	{	
		if(current->allocated) {
			used+=current->size;
		}
		else {
			freemem+=current->size;
		}
		current = current->next;
	}
	
	print_color(fetch_logo[0],0b1110);
	for(int i = strlen(fetch_logo[0]);i<25;i++) putchar(' ');
	print_color("[ZHIR OS]\n",0b1110);

	char *cpu_model = kalloc(64);
        get_cpu_model(cpu_model);
        
	print_color(fetch_logo[1],0b1110);
	for(int i = strlen(fetch_logo[1]);i<25;i++) putchar(' ');
	
	print_color("CPU:",0b0011);
	print(cpu_model);
        print("\n");
	free(cpu_model);
	

	print_color(fetch_logo[2],0b1110);
	for(int i = strlen(fetch_logo[2]);i<25;i++) putchar(' ');
	
	print_color("RAM free:", 0b0010);
	print_int(freemem>>20);
	print("MB\n");
	
	print_color(fetch_logo[3],0b1110);
	for(int i = strlen(fetch_logo[3]);i<25;i++) putchar(' ');
	
	print_color("allocated:", 0b0110);
	print_int(used);
	print("\n");
	
	print_color(fetch_logo[4],0b1110);
	for(int i = strlen(fetch_logo[4]);i<25;i++) putchar(' ');
	
	print_color("ramdisk/kernel:", 0b0100);
	print_int(ramdisk_size>>20);
	print("MB\n");

	print_color(fetch_logo[5],0b1110);
	for(int i = strlen(fetch_logo[5]);i<25;i++) putchar(' ');
	print("\n");
	return 0;
}

struct object *date(struct objectArray* args)
{
	struct rtc_time now = get_time();

        print_int(now.day);
	print(".");
	print_int(now.month);
	print(".");
	print_int(now.year+2000);
	
	print("   ");
	
	print_int(now.hour);
        print(":");
        print_int(now.minute);
        print(":");
        print_int(now.second);
        print("\n");

	return 0;
}

struct object *help(struct objectArray* args)
{
	struct function_info *current = head_fnc;
	while(current){
		print(current->name);
		print(",");
		current = current->next;
	}
	print("\n");
	return 0;
}

struct object *lsblk(struct objectArray* args)
{
	for(int i = 0;i<MAX_DISK_COUNT;i++)
	{
		if(!disks[i])continue;
		print(disks[i]->name);
		print("    ");
		putchar(i+'A');
		print(":\n");
	}
	return 0;
}

struct object *dump_disk(struct objectArray*args)
{
	if(args->count<3){
		KLOGE("use dmpdsk <disk letter> <lba> <len>\n");
		return 0;
	}
	int idx = *(char*)args->objs[0].data-'a';
	int lba = str2int(args->objs[1].data);
	int len = str2int(args->objs[2].data);
	int blocks = len/512;
	if(len%512)blocks+=1;
	
	char *buffer = kalloc(blocks*512);
	disks[idx]->lba_read(disks[idx]->id,lba,buffer,blocks);
	
	hexdump(buffer,len);	
	free(buffer);
	return 0;
}

struct object *lspci(struct objectArray* args)
{
	pci_scan();
}
struct object *cat(struct objectArray* args)
{
	if(args->count <2){
		KLOGE("use cat <diskletter> <file name>\n");
		return 0;
	}
	int diskid = *(char*)args->objs[0].data-'a';
	struct disk *dsk =disks[diskid];
	BPB* bpb = read_first_sector(dsk);
	RootDir* rootdir=calculateRootDir(bpb);
	u8* root_buffer = readRootDir(dsk,rootdir,bpb);
	DirEntry *file = find_file(root_buffer,bpb,args->objs[1].data);
	if(!file) 
	{
		KLOGE("file not found\n");
		free(bpb);free(rootdir);free(root_buffer);
	
		return 0;
	}
	void *buffer = kalloc(get_file_size(file,bpb));
	load_file(dsk,file,bpb,rootdir,buffer);

	print(buffer);

	free(bpb);free(rootdir);free(root_buffer);free(file);free(buffer);

	return 0;
}
struct object *hexdump_cmd(struct objectArray* args)
{
	if(args->count <2){
		KLOGE("use hexdump <diskletter> <file name>\n");
		return 0;
	}
	int diskid = *(char*)args->objs[0].data-'a';
	struct disk *dsk =disks[diskid];
	BPB* bpb = read_first_sector(dsk);
	RootDir* rootdir=calculateRootDir(bpb);
	u8* root_buffer = readRootDir(dsk,rootdir,bpb);
	DirEntry *file = find_file(root_buffer,bpb,args->objs[1].data);
	if(!file) 
	{
		KLOGE("file not found\n");
		free(bpb);free(rootdir);free(root_buffer);
	
		return 0;
	}
	void *buffer = kalloc(get_file_size(file,bpb));
	load_file(dsk,file,bpb,rootdir,buffer);

	hexdump(buffer,file->file_size);

	free(bpb);free(rootdir);free(root_buffer);free(file);free(buffer);

	return 0;
}

struct object *ls(struct objectArray* args)
{
	if(args->count <1){
                KLOGE("use ls <diskletter>\n");
                return 0;
        }
        int diskid = *(char*)args->objs[0].data-'a';
        struct disk *dsk =disks[diskid];
        BPB* bpb = read_first_sector(dsk);
        RootDir* rootdir=calculateRootDir(bpb);
        u8* root_buffer = readRootDir(dsk,rootdir,bpb);
        print_dir(root_buffer,bpb);
	
	free(bpb);free(rootdir);free(root_buffer);

        return 0;

}

void main(){
	find_disk();
	
	
	register_function("echo",echo);
	register_function("cat",cat);
	register_function("ls",ls);
	register_function("hexdump",hexdump_cmd);
	register_function("lspci",lspci);
	register_function("screenfetch",screenfetch);
	register_function("date",date);
	register_function("lsblk",lsblk);
	register_function("dmpdsk",dump_disk);
	register_function("clear",clear);
	register_function("help",help);

	KLOGI("system functions registered\n");	

	start_shell();
}

#include "multiboot.h"
