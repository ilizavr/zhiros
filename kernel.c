#include "int_types.h"
#include "ports.h"
#include "print.h"
#include "allocator.h"
#include "keyboard.h"
#include "function_manager.h"
#include "shell.h"
#include "cpu.h"
#include "time.h"

struct object *print_objects(struct objectArray* args)
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

	print_color("[ZHIR OS]\n",0b1110);

	char *cpu_model = kalloc(64);
        get_cpu_model(cpu_model);
        print_color("CPU:",0b0011);
        print(cpu_model);
        print("\n");
	free(cpu_model);
	
	print_color("memory free:", 0b0010);
	print_int(freemem>>20);
	print("MB\n");
	print_color("memory used:", 0b0100);
	print_int(used);
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

void main(){
	register_function("print",print_objects);
	register_function("screenfetch",screenfetch);
	register_function("date",date);
	register_function("clear",clear);
	register_function("help",help);
	start_shell();
}

#include "multiboot.h"
