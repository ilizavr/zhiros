short *video = (short*)0xB8000;

#include "int_types.h"
u32 ticks = 0;
#include "ports.h"
#include "serial.h"
#include "print.h"
#include "allocator.h"
#include "interrupt.h"
#include "cpu.h"
#include "process.h"
#include "mouse.h"
#include "fb.h"
#include "keyboard.h"
#include "function_manager.h"
#include "shell.h"
#include "time.h"
#include "disk.h"
#include "ramdisk.h"
#include "neuro_ide.h"
#include "pci.h"
#include "fat16.h"
#include "rand.h"

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
	print("M\n");
	
	print_color(fetch_logo[3],0b1110);
	for(int i = strlen(fetch_logo[3]);i<25;i++) putchar(' ');
	
	print_color("allocated:", 0b0110);
	print_int(used>>10);
	print("k\n");
	
	print_color(fetch_logo[4],0b1110);
	for(int i = strlen(fetch_logo[4]);i<25;i++) putchar(' ');
	
	print_color("ramdisk/kernel:", 0b0100);
	print_int(ramdisk_size>>20);
	print("M\n");

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
		for(int i = strlen(current->name);i<20;i++) putchar(' ');
		print_color(current->description,0b0011);
		print("\n");
		current = current->next;
	}
	print("windows manager     ");
	print_color("press ctrl+num to select window\n",0b0101);
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
	*(char*)buffer = 0;
	load_file(dsk,file,bpb,rootdir,buffer);

	print(buffer);

	free(bpb);free(rootdir);free(root_buffer);free(file);free(buffer);

	return 0;
}
struct image*curimg=0;

struct object *img(struct objectArray* args)
{
	if(args->count <2){
		KLOGE("use img <diskletter> <file name>\n");
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
	if(curimg)free(curimg);
	curimg = kalloc(get_file_size(file,bpb));
	load_file(dsk,file,bpb,rootdir,(void*)curimg);

	free(bpb);free(rootdir);free(root_buffer);free(file);

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

	free(bpb);free(rootdir);free(root_buffer);free(buffer);

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
struct object *random(struct objectArray* args)
{
	if(args->count<2)
	{
		KLOGE("use random <min> <max>\n");
		return 0;
	}

	u32 min = str2int(args->objs[0].data);	
	u32 max = str2int(args->objs[1].data);	

	u32 random = min+ (rand() % (max-min+1));
	print_int(random);
	print("\n");
	
	return 0;

}
struct object *dump_mem(struct objectArray*args)
{
	if(args->count<2){
                KLOGE("use dmpmem <start> <end>\n");
                return 0;
        }
        u32 start = str2int(args->objs[0].data);
        u32 end = str2int(args->objs[1].data);

        hexdump((void*)start,end-start);
        return 0;
}

struct object* uptime(struct objectArray *args)
{
	u32 all = ticks/1000;
	u32 sec = all%60;
	all/=60;
	u32 min = all%60;
	all/=60;
	u32 hour = all;

	if(hour>0){print_int(hour);print("h ");}
	if(min>0){print_int(min);print("m ");}
	print_int(sec);print("s\n");
	return 0;
}

struct object* touch(struct objectArray *args)
{
	if(args->count<2)
	{
		KLOGE("use touch <diskletter> <filename>");
		return 0;
	}

	int diskid = *(char*)args->objs[0].data-'a';
	struct disk *dsk =disks[diskid];
	BPB* bpb = read_first_sector(dsk);
	RootDir* rootdir=calculateRootDir(bpb);
	u8* root_buffer = readRootDir(dsk,rootdir,bpb);
	
	DirEntry* file = create_file(root_buffer,bpb,args->objs[1].data);
	if(!file && file != LONG_NAME)
	{
		KLOGE("cant create file\n");

		free(bpb);free(rootdir);free(root_buffer); free(file);
	
		return 0;
	} else if(file == LONG_NAME) 
	{
         KLOGE("the file name is too long");

        free(bpb);free(rootdir);free(root_buffer); free(file);
	return 0;
	}

	writeRootDir(dsk,rootdir,bpb,root_buffer);
	
	free(bpb);free(rootdir);free(root_buffer);
	return 0;
}
struct object *wf(struct objectArray* args)
{
	if(args->count <3){
		KLOGE("use wf <diskletter> <file name> <data>\n");
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

        file = create_file(root_buffer,bpb,args->objs[1].data);
	if(!file && file != LONG_NAME)
	{
		KLOGE("file not found and cant create of it\n");

		free(bpb);free(rootdir);free(root_buffer); free(file);
	
		return 0;
	} else if(file == LONG_NAME) 
	{
         KLOGE("the file name is too long");

        free(bpb);free(rootdir);free(root_buffer); free(file);
	return 0;
	}

	}
	write_file(dsk,file,bpb,rootdir,args->objs[2].data,strlen(args->objs[2].data));

	writeRootDir(dsk,rootdir,bpb,root_buffer);
	free(bpb);free(rootdir);free(root_buffer);free(file);

	return 0;
}

struct object *ps(struct objectArray *args)
{
	mouse_init();

	print("PID | name\n");
	for(int i = 0;i<MAX_TASK_COUNT;i++)
	{
		if(!tasks[i])continue;
		print_int(i);
		print("     ");
		print(tasks[i]->name);
		print("\n");
	}	
}


void emptyprocess()
{
	while(true)asm volatile("hlt");
}
s32 old_mouse_x=0,old_mouse_y=0;

void testdrawframe()
{
	put_text("mouse demo",0,0,0xAA0088,0);
	
	if (mouse_x == old_mouse_x && mouse_y == old_mouse_y) return;
    	
	for(int x = old_mouse_x-8;x<old_mouse_x+16;x++)for(int y = old_mouse_y-16;y<old_mouse_y+32;y++){
		put_pixel(x,y,0);
	}
	
	put_sym('^',mouse_x,mouse_y,0xFFFFFFFF,0);
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;
}

void testdrawimage()
{
	put_text("image demo",0,0,0xAA0088,0);
	if(curimg)drawimage(curimg,0,20);
}

extern void keyboard_isr_handler();

void main(){
	pic_remap();
	init_idt();
	set_idt_gate(33, (u32)keyboard_isr_handler);
	init_timer();
	init_cpu_exception();
	
	find_pci_devices();

	mouse_init();

	register_function("random", random,"random value");
	register_function("img",img,"view image from file");
	register_function("wf",wf,"write to file");
	register_function("touch",touch,"create file");
	register_function("hexdump",hexdump_cmd,"print file in hexview");
	register_function("cat",cat,"print file to console");
	register_function("ls",ls,"print files in dirrectory");
	register_function("dmpdsk",dump_disk,"lowlevel dump disk sectors");
	register_function("dmpmem",dump_mem,"lowlevel dump RAM");
	register_function("lspci",lspci,"print all pci device info");
	register_function("lsblk",lsblk,"print all disk");
	register_function("ps",ps,"list process");
	register_function("uptime",uptime,"print system uptime");
	register_function("date",date,"print date and time");
	register_function("fetch",screenfetch,"short system information");
	register_function("clear",clear,"clear screen");
	register_function("echo",echo,"print to console");
	register_function("help",help,0);
	
	KLOGI("system functions registered\n");	
	
	create_process((u32)windowsmanager,"windows manager",0);
	create_process((u32)start_shell,"shell",ega2fb);
	create_process((u32)process_mouse,"mouse demo",testdrawframe);
	create_process((u32)emptyprocess,"image demo",testdrawimage);

	asm volatile ("sti");
}

#include "multiboot.h"
