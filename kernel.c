short *video = (short*)0xB8000;


#include "config.h"
#include "int_types.h"
bool dont_interrupt_me = false;
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
#include "acpi.h"
#include "bios.h"
#include "vfs.h"

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
	print_color("OS: ",0b0011);
	print("ZHIROS\n");

	print_color(fetch_logo[1],0b1110);
	for(int i = strlen(fetch_logo[1]);i<25;i++) putchar(' ');
	
	print_color("Uptime: ",0b0011);
	u32 all = ticks/1000;
        u32 sec = all%60;
        all/=60;
        u32 min = all%60;
        all/=60;
        u32 hour = all;

        if(hour>0){print_int(hour);print("h ");}
        if(min>0){print_int(min);print("m ");}
        print_int(sec);print("s\n");
	

	print_color(fetch_logo[2],0b1110);
	for(int i = strlen(fetch_logo[2]);i<25;i++) putchar(' ');
	
	print_color("Display: ", 0b0011);
	print_int(screen_width);
	print("x");
	print_int(screen_height);
	print("\n");
	
	print_color(fetch_logo[3],0b1110);
	for(int i = strlen(fetch_logo[3]);i<25;i++) putchar(' ');
	

	char *cpu_model = kalloc(64);
        get_cpu_model(cpu_model);
        
	print_color("CPU: ", 0b0011);
	print(cpu_model);
	print("\n");
	free(cpu_model);	

	print_color(fetch_logo[4],0b1110);
	for(int i = strlen(fetch_logo[4]);i<25;i++) putchar(' ');
	
	print_color("Memory:", 0b0011);
	print_int(used>>10);
	print("k/");
	print_int((used+freemem)>>20);
	print("M\n");

	print_color(fetch_logo[5],0b1110);
	for(int i = strlen(fetch_logo[5]);i<25;i++) putchar(' ');
	print("\n");

	for(int i = 0;i<25;i++)putchar(' ');
	for(int i = 0;i<16;i++)print_color(" ",i<<4);
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
	print("NAME                CODE    SIZE\n");

	for(int i = 0;i<MAX_DISK_COUNT;i++)
	{
		if(!disks[i])continue;
		print(disks[i]->name);
		for(int j = strlen(disks[i]->name);j<20;j++) putchar(' ');
		putchar(i+'a');
		print("       ");
		int sizeinmb = disks[i]->size>>11;
		if(sizeinmb>1024)
		{
			print_int(sizeinmb>>10);
			print("G\n");
		}else{
			print_int(sizeinmb);
			print("MB\n");
		}
		for(int j = 0;j<4;j++)
		{
			if(disks[i]->partitions[j].total_sectors==0)continue;
			print("  |--part");
			print_int(j);
			print("          ");
			putchar(i+'a');
			print_int(j);
			print("      ");
			sizeinmb = disks[i]->partitions[j].total_sectors>>11;
			if(sizeinmb>1024)
                	{       
                        	print_int(sizeinmb>>10);
                        	print("G\n");
                	}else{
                        	print_int(sizeinmb);
                        	print("MB\n");
                	}

		}
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
	if(args->count <1){
		KLOGE("use cat <path>\n");
		return 0;
	}

        u64 size = 0;
	void *buffer = kalloc(5000);	
	write_buf(buffer,args->objs[0].data,11);
	u64 res = read(STUB_FD,buffer,size);
	
	if(res == 0)return 0;
	
	print(buffer);

	free(buffer);

	return 0;
}
struct image*curimg=0;

struct object *img(struct objectArray* args)
{
	if(args->count <1){
		KLOGE("use img <path>\n");
		return 0;
	}
	if(curimg)free(curimg);
	
	u64 size = 0;
	void *buffer = kalloc(5000);
	
	u64 res = read(STUB_FD,buffer,size);
       
	if(res == 0)return 0;

	return 0;
}
struct object *hexdump_cmd(struct objectArray* args)
{
	if(args->count <1){
		KLOGE("use hexdump <path>\n");
		return 0;
	}
	 u64  size = 0;
    
	void *buffer = kalloc(5000);
	u64 res = read(STUB_FD,buffer,size);
        
	if(size == 0)return 0;

	hexdump(buffer,size);

	free(buffer);

	return 0;
}

struct object *seldisk_cmd(struct objectArray *args)
{
	if(args->count <1){
                KLOGE("use seldisk <code>\n");
                return 0;
        }
	seldisk(args->objs[0].data);
	return 0;
}

struct object *ls(struct objectArray* args)
{
        struct disk *dsk =disks[current_diskid];
        BPB* bpb = read_first_sector(current_partstart,dsk);
        RootDir* rootdir=calculateRootDir(bpb);
        u8* root_buffer = readRootDir(current_partstart,dsk,rootdir,bpb);
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
                KLOGE("use dmpmem <start> <len>\n");
                return 0;
        }
        u32 start = str2int(args->objs[0].data);
        u32 len = str2int(args->objs[1].data);

	print_hex(start);
	print("\n");

        hexdump((void*)start,len);
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
	if(args->count<1)
	{
		KLOGE("use touch <filename>");
		return 0;
	}

	write(STUB_FD,args->objs[0].data,0,0);

	return 0;
}
struct object *wf(struct objectArray* args)
{
	if(args->count <2){
		KLOGE("use wf <path> <data>\n");
		return 0;
	}
	write(STUB_FD,args->objs[0].data,args->objs[1].data,strlen(args->objs[1].data));

	return 0;
}

struct object* mkdirn(struct objectArray* args)
{
if(args->count<1)
{
KLOGE("use mkdir <dirname>\n");
return 0;
}

mkdir(args->objs[0].data,STUB_MODE);
return 0;
}


struct object *kill_cmd(struct objectArray *args)
{
	if(args->count<1){
		KLOGE("use kill <pid>\n");
		return 0;
	}
	
	int pid = str2int(args->objs[0].data);
	kill(pid);

	return 0;
}
struct object *sleep_cmd(struct objectArray *args)
{
	if(args->count<1){
		KLOGE("use sleep <millisecond>\n");
		return 0;
	}
	
	int m = str2int(args->objs[0].data);
	sleep(m);

	return 0;
}
struct object *ps(struct objectArray *args)
{

	print("PID | name\n");
	for(int i = 0;i<MAX_TASK_COUNT;i++)
	{
		if(!tasks[i])continue;
		print_int(i);
		print("     ");
		print(tasks[i]->name);
		print("\n");
	}

	return 0;	
}
struct object *reboot(struct objectArray *args)
{
	asm volatile("cli");
	u8 good = 0x02;
	while(good&0x02)good=inb(0x64);	

	outb(0x64,0xfe);

	return 0;	
}

struct object *poweroff(struct objectArray *args)
{
	int slptypea = 0,slptypeb = 0;

	if(args->count>=2){
		slptypea = str2int(args->objs[0].data);
		slptypeb = str2int(args->objs[1].data);
	}
	else if(args->count==1){
		slptypea = str2int(args->objs[0].data);
		slptypeb = str2int(args->objs[0].data);
	}

	asm volatile("cli");
	asm volatile("wbinvd");

	outw(fadt->PM1aControlBlock, (slptypea << 10) | (1 << 13));
	if (fadt->PM1bControlBlock != 0) {
		outw(fadt->PM1bControlBlock, (slptypeb << 10) | (1 << 13));
	}

	asm volatile("sti");
	reboot(0);

	return 0;
}


void emptyprocess()
{
	while(true)asm volatile("hlt");
}
s32 old_mouse_x=0,old_mouse_y=0;

void testdrawmouse()
{
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
	if(curimg)drawimage(curimg,0,17);
}

extern void keyboard_isr_handler();

void main(char *cmdline){
#ifdef BIOSDISK
	init_bios();
#endif

	enableacpi();

	pic_remap();
	init_idt();
	set_idt_gate(33, (u32)keyboard_isr_handler);
	init_timer();
	init_cpu_exception();
	
	find_pci_devices();

#ifdef MOUSE
	mouse_init();
#endif

	register_function("reboot", reboot,0);
	register_function("poweroff", poweroff,0);
	register_function("random", random,"random value");
	register_function("img",img,"view image from file");
	register_function("wf",wf,"write to file");
	register_function("mkdir",mkdirn,"make a directory,or throw a warning if it already exists");
	register_function("touch",touch,"create file");
	register_function("hexdump",hexdump_cmd,"print file in hexview");
	register_function("cat",cat,"print file to console");
	register_function("ls",ls,"print files in dirrectory");
	register_function("seldisk",seldisk_cmd,"select disk");
	register_function("dmpdsk",dump_disk,"lowlevel dump disk sectors");
	register_function("dmpmem",dump_mem,"lowlevel dump RAM");
	register_function("lspci",lspci,"print all pci device info");
	register_function("lsblk",lsblk,"print all disk");
	register_function("ps",ps,"list process");
	register_function("kill",kill_cmd,"kill process");
	register_function("uptime",uptime,0);
	register_function("sleep",sleep_cmd,"test sleep");
	register_function("date",date,"print date and time");
	register_function("fetch",screenfetch,"short system information");
	register_function("clear",clear,"clear screen");
	register_function("echo",echo,"print to console");
	register_function("help",help,0);
	
	KLOGI("system functions registered\n");	
	
	create_process((u32)windowsmanager,"windows manager",0);
	create_process((u32)start_shell,"shell",ega2fb);
#ifdef MOUSE
	create_process((u32)process_mouse,"mouse demo",testdrawmouse);
#endif
	create_process((u32)emptyprocess,"image demo",testdrawimage);

	is_interrupt_enabled = true;
	asm volatile ("sti");
}

#include "multiboot.h"
