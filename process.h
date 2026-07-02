struct Task
{
	char *name;
	u32 esp;
	void* start_esp;
	void (*drawframe)();
};

#define MAX_TASK_COUNT 10
#define PROCESS_STACK_SIZE 2048

struct Task *tasks[MAX_TASK_COUNT] = {0};
int current_process_idx = MAX_TASK_COUNT-1;
bool first_switch = true;

extern void context_switch(u32,u32*);

void task_entry_stub(u32 func)
{
	asm volatile ("sti");
	void (*f)() = (void(*)())func;
	f();
	while(1)
	{
		asm volatile ("hlt");
	}
}

void kill(int pid)
{
	asm volatile("cli");

	free(tasks[pid]->start_esp);
	free(tasks[pid]->name);
	free(tasks[pid]);
	tasks[pid] = 0;
	pic_eoi();
	asm volatile("sti");
}


void create_process(u32 func,char *name,void(*drawframe)())
{
	for(int i = 0;i<MAX_TASK_COUNT;i++)
	{
		if(tasks[i]!=0) continue;

		tasks[i] = kalloc(sizeof(struct Task));

		tasks[i]->name = strdup(name);
		tasks[i]->start_esp = kalloc(PROCESS_STACK_SIZE);
		u32 *stack = (u32*)((u32)tasks[i]->start_esp+ PROCESS_STACK_SIZE);

		stack--; *stack = func;
		stack--; *stack = 0;
		stack--; *stack = (u32)task_entry_stub;
		stack--; *stack = 0;
		stack--; *stack = 0;
		stack--; *stack = 0;
		stack--; *stack = 0;

		tasks[i]->esp = (u32)stack;
		tasks[i]->drawframe = drawframe;
		return;
	}
}

void schedule()
{
	int idx = current_process_idx;
	int old_idx = current_process_idx;

	while(1)
	{
		idx = (idx+1)%MAX_TASK_COUNT;
		if(!tasks[idx]) continue;
		break;
	}

	if(idx==current_process_idx && !first_switch) {
		return;
	}

	current_process_idx = idx;

	if(first_switch)
	{
		u32 dummyesp = 0;
		first_switch = false;
		context_switch(tasks[idx]->esp,&dummyesp);
		return;
	}

	context_switch(tasks[idx]->esp,&tasks[old_idx]->esp);
}
