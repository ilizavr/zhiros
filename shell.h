static struct objectArray* objs = 0;
void start_shell()
{
	print("shell started. type help to get command list\n");
	while(true)
	{
		char buffer[64];
		char *argv[64];
		*argv = kalloc(64);
		int argc = 0;
		int j = 0;
		
		print(">");
		input(buffer,64);
		
		for(int i = 0;i<64;i++)
		{
			if(buffer[i] == 0){ argv[argc][j] = 0; break; }
			
			if(buffer[i] == ' '){ 
				argv[argc][j] = 0; 
				argc++; 
				j=0; 
				argv[argc] = kalloc(64);
			}
			else {argv[argc][j]=buffer[i]; j++; }
		}
		if(!argv[0][0]) {
			for(int i = 0;i<=argc;i++)free(argv[i]);
			continue;
		}
		if(!objs) objs = kalloc(64*sizeof(struct objectArray));
		for(int i = 0;i<argc;i++)
		{
			objs->objs[i].data = argv[i+1];
			objs->objs[i].type = OBJECT_STRING;
			objs->objs[i].len = strlen(argv[i+1]);
		}
		objs->count=argc;

		function some = resolve_function(argv[0]);
		if(some) some(objs);
		for(int i = 0;i<=argc;i++)free(argv[i]);
	}
}
