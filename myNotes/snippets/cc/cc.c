//TODO: i planned in day write these functions:
//*isdigit/alpha/space/keyword/operation/pointerwork/operator()
//*skip_space(),tokenize_digit()
//*skip_block_comment()
//*skip_one_comment() 



//this compiler needed for Nokia 106(called as cc)
//it have next pipeline:

//source code -> simple preprocessor -> tokenizer -> dynamic array of tokens -> codegen reads that array and directly generates ARM-machine code. without linkers and
//assemler compilers

//constraint: proggram can't be greate than 100 00 tokens

//for example,cc consumes 50 KB of RAM when proggram is 100 00 tokens. 10 KB on reading the file into buffer,and 40 KB on store proggram's text as token sequince

//why i so economed on memory usage if it very tiny even for embedded compiler? because i was want to embed SSA,CFG,CF,DCE,and other hard optimizations

//the cc compiler makes the standard libc library from scratch to aim in our OS development on pure logic,syscalls,abstraction implementation,maybe caching

//nokia 106 have no MMU. OS have no virtual addreses. that's why kmalloc/kfree only as allocators

//the code below just snippet of lexer i planned


// syscall.h is a header of OS we developed(Unix-like os on Nokia 106 with this compiler,terminal,base utilits,edit,etc) for system calls
// for example to get access to OS's allocator(kmalloc/kfree methods only) you need #include<allocator.h>
#include<syscall.h>

#define STDOUT 1
#define IO_FAILED -1
#define IO_SUCCESS 0

// token has the correspond structure:
// [0 BYTE RESERVED] [1-2 BYTE LINE WHERE TOKEN] [3 BYTE TOKEN TYPE(e.g TINT,TCHAR,TIF,etc)
typedef int token_t;

char len(char* str){
char l = 0;

while(*str){
l++;
}
return l;
}

signed char put_char(char c){
char code = write(STDOUT,c,1);
  
if(code == IO_FAILED){
  return IO_FAILED;
  }   
  return IO_SUCCESS;
}

signed char puts(char* str){
char code = write(STDOUT,str,len(str));

if(code == IO_FAILED){
return IO_FAILED;
}
return IO_SUCCESS;
}


void printf(char* fmt,...){
va_list args;
va_start(args,fmt);

int pos = 0;

for(char* p = fmt;*p;p++){
++pos;
if(*p == '%'){
switch(*p++){
	case 'd': 
	//write code
	case 'x':
	//write code
	case 's':
	//write code
	case 'c':
	//write code	
}

} else{
signed char status_code = put_char(*p);

if(status_code == IO_FAILED){
exit(1);
}
}
}
}


  noreturn void errorf(char *line, char *pos, char *fmt, ...);
  void warnf(char *line, char *pos, char *fmt, ...);


