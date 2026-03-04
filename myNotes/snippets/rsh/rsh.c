//here's nothing i was want todo. if you want use it as well as base of your shell,you it. but know i not planned the future for this project
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


#define MAX_ARGS 60
#define EXIT_SUCSESS 0
#define EXIT_FAILURE 1
#define RSH_CURR_VERSION "0.0.7"
#define STDIN 0
#define STDOUT 1
#define PROMPT "rsh@desktop$ "

void signal_handler(int sig)
{
if(sig == 0x2)
{
kill(-1,SIGKILL);
return;
}

if(sig == 0x14)
{
kill(-1,SIGTSTP);
// потом, добавь обработку восстановления процессов
}

if(sig == 0xb)
{
puts("segmention fault");
kill(-1,SIGKILL);
raise(SIGKILL);
}
if(sig == 0xF)
{
puts("the rsh now terminates");

Builitins = {NULL};

buf = {NULL};

args = {NULL};

raise(SIGTERM);

}
}


char* Builitins[] = {"cd","exit","which","echo","rshinfo","~",NULL};
char buf[256];
char *args[MAX_ARGS] = {NULL};

bool isBuilitin(char* c)
{
    for(int i = 0; Builitins[i] != NULL;i++)
    {
        if(strcmp(Builitins[i],c) == 0)
        {
            return 1;
        }

    }
    return 0;
}

bool isPipeLine(char args[])
{
    for(int i = 0;i < strlen(args);i++)
    {
        if(args[i] == '|')
        {
           return 1; 
        }
    }
    return 0;
}

int exec_pipeline(char* cmd[],char* cmd1)
{

    pid_t p_one = fork();

    if(p_one == 0)
    {
        int fd[2];

        int res_p = pipe(fd);

        if(res_p == -1)
        {
            return -1;
        }
        // мне кажется на этой строке может быть баг. будь остторожен
        int f2 = dup2(STDOUT,fd[1]);

        pid_t p_two  = fork();

        switch(p_two)
        {
            case -1:
            {
            return -1;
            }
            case 0:
            {
            close(fd[0]);
            int max_args = MAX_ARGS << 2;

            char buf[max_args];

            int r = read(fd[0],buf,max_args);

            if(r == -1)
            {
                return -1;
            }

           ssize_t w =  write(STDIN,buf,r);

           if(w == -1)
           {
            return -1;
           }

           execvp(cmd1[0],cmd1);
           exit(EXIT_FAILURE);
        }

           default:
           {
           execvp(cmd[0],cmd);
           exit(EXIT_FAILURE);
        }
    }
    }
}

void concat(char* buf,const char *s1, const char *s2) 
{
    char s1_copy[strlen(s1)];
    strcpy(s1_copy,s1);

  int i = 0;
  int j = 1;
  int s_len = (int)strlen(s1);

  s1_copy[s_len] = 'l';

  for(;i < s_len; i++)
  {
    buf[i] = s1_copy[i];
  }

  buf[i] = *s2;

++i;
  for(;j < strlen(s2);j++)
  {
    buf[i] = s2[j];
    ++i;
  }
}

void builitin_base(char* cmd)
{
    if(strcmp(cmd,"exit") == 0) {
        exit(0);
    }
    else if(*cmd == '~') {
        chdir(getenv("HOME"));
        return;
    }
    else if(strcmp(cmd,"rshinfo") == 0) {
        printf("rsh - the simple shell, created by 13 years teenager. current version:%s \n", RSH_CURR_VERSION);
        return;
    }
}

void builitin_cd(char* path)
{
    if(path == NULL)
        {
            chdir(getenv("HOME"));
           return;
        }
        else
        {
            chdir(path);
           return;
        }
 }
 
void builitin_which(char* cmd)
{

    if(cmd == NULL)
        {
            printf("the which command must have an argument");
            return;
        }
        else 
        {
            if(isBuilitin(cmd))
            {
                printf("the %s command has been builitin for rsh \n",cmd);
                return;
            }
            else if (!isBuilitin(cmd))
            {
                char conc[strlen("/usr/bin") + strlen(cmd)];

                concat(conc,"/usr/bin/",cmd);

                int e = access(conc,F_OK | X_OK);

                if(e == 0)
                {
                    printf("%s \n",conc);
                  
                }
                else
                {
                    printf("the %s command doesn't exist \n",cmd);
            
                }
            }
        }
    
}

void builitin_echo(char* words[])
{
    for(int i = 1;words[i][i] != '\0';i++)
    {
     printf("%s \n",words[i][i]);
    }
}

void input_wait()
{
ssize_t r = read(STDIN,buf,255);
     
if(r == -1)
{
perror("failed to read an input from the stream output");
} 

buf[r] = '\0';
}

void parse_command_and_check_env(char* cmd)
 {
    int i = 0;
    char *token = strtok(cmd, " \n");

    while (token != NULL) {
        if(isPipeLine(token))
        {
            // do something,later
        }

        if (*token == '$')
         {
            char* env = getenv(++token);
            --token; 
            if(env == NULL)
            {
                args[i++] = "";
    
                continue;
            }
            args[i++] = env;
    
            continue;
        }
        if(*token != '$')
        {
        args[i++] = token;
        }
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
}

bool define_a_buitin(char* cmd, char* args[])
{
    if(strcmp(cmd,"which") == 0) {
        builitin_which(args[1]);
        return 1;
    }
    else if(strcmp(cmd,"echo") == 0) {
        builitin_echo(args);
        return 1;
    }
    else if(strcmp(cmd,"cd") == 0) {
        builitin_cd(args[1]);
        return 1;
    }
    
    builitin_base(cmd);
    return 1;
}
void exec_an_command(char** args) 
{
   bool b =  define_a_buitin((args[0]),args);
    if(!b)
    {
    pid_t pid = fork();

    if (pid == 0) {
        execvp((args[0]), args);
        exit(EXIT_FAILURE);
    } else {
        waitpid(pid, NULL, 0);
    }
}
}

int main()
{
    while(1)
    {
        printf(PROMPT);
        fflush(stdout);

        input_wait();
        parse_command_and_check_env(buf);
        exec_an_command(args);

          for(int i = 0; i< MAX_ARGS;i++)
        {
            args[i] = 0;
        }
          for(int i = 0; i< 256;i++)
        {
            buf[i] = 0;
        }
    }

}

