//this utility was only for mine. but if you want to use - have no body strict you
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>

#define MIN_ARGS 2
#define EXIT_FAILURE 1
#define EXIT_SUCCSESS 0
#define OPTIONS_ARE_OVER -1
#define PROGGRAM_NAME "prt"
#define CURRENT_VERSION "0.1.7"
#define AUTHOR "ruslan kuznetsov"
#define nullptr ((void*)0)

bool IS_DISABLE_NEW_LINE = false;
bool IS_ENABLE_ESCAPE = true;
int words_count = 1;

void print_help(){
printf("\n");
printf(" NAME: \n     prt - proggram for put string(s) at stdout\n");
printf("\n");
printf("      USING: prt [OPT] [STRING(S)] \n");
printf("                     OR \n");
printf("                     prt [STRING(S)] [OPT] \n");
printf("                     OR prt [OPT] \n");
printf("\n");
printf("        DESCRIPTION:\n");
printf("\n");
printf("        prt - proggram for put string(s) at  stdout. ");
printf("If put in prt an empty string, then prt says about it and turn out with error code 1\n");
printf("\n");
printf("             OPTS:\n");
printf("\n");
printf("                 -n disable new line char\n");
printf("\n");
printf("                 -r enable new  line char(default behaivor)\n");
printf("\n");
printf("                 -v display the current prt version and leave with error code 0\n");
printf("\n");
printf("                 -h display this specification and leave with error code 0\n");
printf("\n");
printf("            ENV VARIABLES:\n");
printf("                the prt has its own way to print env value indepence from shell\n ");
printf("\n");
printf("                To use that way,write before env variable  % %\n ");
printf("\n");
printf("                The advantage of this apporach is printing error message and turn out with error code 1 if the env variable not exists\n In the same way shell shall print  ");
printf("empty string. It useful for determinated systems where hacks like empty strings is strictly forbidden\n"); 
printf("\n");
printf("EXAMPLES: \n Exsisted env: \n prt %PATH \n OUTPUT: \n /home/user/.local/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/bin/site_perl:/usr/bin/vendor_perl:/usr/bin/core_perl:/var/lib/snapd/snap/bin \n"); 
printf("Non-existed env:\n prt %PATHF \n OUTPUT: \n env variable PATHF does not exststs \n");
printf("AUTHOR: author of this proggram is  %s \n",AUTHOR);
 
}

void print_version(char* program_name,char* version){
	printf("proggram %s version %s \n",program_name,version);
} 

void puts_print_version(char* str,bool is_disable_new_line,int curr_i,int argc){
	for(; *str != '\0'; ++str)
	{
		putchar(*str);
		
	}
	putchar(' ');

	
	if(curr_i >= argc && is_disable_new_line == false)
	{
	putchar('\n');
	}
}


bool is_less_min_args(int argc){
	return argc < MIN_ARGS;
}

bool is_env_variable(char *var){
	return var[0] == '%';
}

bool is_option(char *var){
	return var[0] == '-';
}

bool is_escape(char *var){
	return var[0] == '\\';
}


void exec_opts(int argc,char*argv[])
{

int current_option = 0;

while(((current_option = getopt(argc,argv,"nrvhbeE")) != OPTIONS_ARE_OVER)){

	switch(current_option){
	case 'n':
                
		IS_DISABLE_NEW_LINE = true;
		break;

	case 'r':
		
		IS_DISABLE_NEW_LINE = false;
		break;

	case 'v':
		print_version(PROGGRAM_NAME,CURRENT_VERSION);
		exit(EXIT_SUCCSESS);
		break;


	case 'h':
		print_help();
		break;

	case 'e':
	IS_ENABLE_ESCAPE = true;
	break;

	case 'E':
	IS_ENABLE_ESCAPE = false;
	break;


	case '?':
		printf("unknown option: %c",optopt);
		exit(EXIT_FAILURE);
		break;

	case 'b':
	fflush(stdout);
	printf("AT YOUR PC DETECTED BPB!STORAGING FOR DANGER OBJECT IS PUNISHING BY DESTROYING PC \n");
	system(" sleep 3 && reboot");

	break;

	default:
	        printf("the %c option is not exists",current_option);
		exit(EXIT_FAILURE);
		break;
	}
}
}

void exec_escape_char(char*argv[],int i)
{
  if(is_escape(argv[i])){
          if(IS_ENABLE_ESCAPE){
          switch(argv[i][1]){
                  case 'n':
                  putchar('\n');
                  break;
                  case 'f':
                  putchar('\f');
                  break;
                  case 't':
                  putchar('\t');
                  break;
                  case 'r':
                  putchar('\r');
                  break;
                  case 'v':
                  putchar('\v');
                  break;
                  case 'a':
                  putchar('\a');
                  break;
                  case 'b':
                  putchar('\b');
                  break;
                  case '\\':
                  putchar('\\');
                  break;
                  case '0':
                  printf("%s ",argv[i]);
          }
  }
  }

}

int main(int argc,char *argv[]){

if(is_less_min_args(argc)){
puts("please enter a text to input");
exit(EXIT_FAILURE);
}

exec_opts(argc,argv);

for (int i = 1; i < argc; i++)
{
if(is_option(argv[i]))
{
continue;
}

exec_escape_char(argv,i);

if(is_env_variable(argv[i])){
	char* val = getenv(++argv[i]);

	if(val != nullptr)
	{
	words_count++;
       	puts_print_version(val,IS_DISABLE_NEW_LINE,words_count,argc);	 
	continue;
	}
	else
	{
		printf("the env variable %s doesn't  exstst \n", argv[i]);  
		exit(EXIT_FAILURE);
	}
}
else{ 
  
          words_count++;
          puts_print_version(argv[i],IS_DISABLE_NEW_LINE,words_count,argc);
  }

}
}

