#define OBJECT_STRING 0

__attribute__((packed)) struct object
{
	u32 type;
	u32 len;
	void* data;
};

__attribute__((packed)) struct objectArray
{
	u32 count;
	struct object objs[0];
};

typedef struct object *(*function)(struct objectArray*); 
__attribute__((packed)) struct function_info
{
	function call;
	char * name;
	char * description;
	struct function_info* next;
};

struct function_info *head_fnc = 0;

function resolve_function(char * function_name)
{
	struct function_info * current = head_fnc;
	while(current)
	{
		if(strcmp(current->name, function_name) == 0) return current->call;
		current=current->next;
	}
	KLOGE("function ");
	print(function_name);
	print(" not found!\n");
	return 0;
}

void register_function(char *function_name, function call, char *description)
{
	struct function_info * newfnc = kalloc(sizeof(struct function_info));
	newfnc->next = head_fnc;
	newfnc->name = strdup(function_name);
	if(description)newfnc->description = strdup(description);
	else newfnc->description = "";
	newfnc->call = call;
	head_fnc = newfnc;
}

