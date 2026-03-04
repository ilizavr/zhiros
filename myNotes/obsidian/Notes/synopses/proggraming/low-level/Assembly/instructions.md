**Warning: some instructions can use and constans and register values. In this synopsys this detail is exclude by reason of  useless**

# instructions for x86_64

- mov [1-rst],[2-nd] - move value from 1-st register into 2-nd register or rever order if you are in AT&T syntax
- add  [1-rst],[2-nd] - add value from 1-st register into 2-nd register or rever order if you are in AT&T syntax
- inc [1-rst] - increment value of register [1-rst]
- dec [1-rst] - decrement value of register [1-rst]
- push [1-rst] - substracting [[registers|rsp]] on 8,and pushing value of [1-rst] register
- pop [1-rst] - adds [[registers|rsp]] on 8,and popping value of top the stack into the [1-rst] register
- sub [1-rst],[2-nd] - substracts value of 1-st register and 2-nd register or rever order if you are in AT&T syntax
- add [1-rst],[2-nd] - adds value of 1-st register and 2-nd register or rever order if you are in AT&T syntax
- jmp [1-rst] - changes value of [[registers|rip]] register on correspondxor addres of [1-rst] label
- xor  and  not  or  shl  shr[1-rst],[2-nd] - makes bit operation with 1-st register and 2-nd register or rever order if you are in AT&T syntax
- lea [1-rst],[2-nd] - load addres of 1-st register into 2-nd register or rever order if you are in AT&T syntax without requests to RAM and changing flags
- mul  div[1-rst],[2-nd] - multiplies/divides values of 1-st register into 2-nd register
- nop - litteraly do **nothing**
- ud2 - generates execption  about illegal hardware instruction(# UD exception)
- movsx [1-rst],[2-nd] -  move value from 1-st register into 2-nd register or rever order if you are in AT&T syntax,but these registers can be variable length. for example 1 operand is ax,second operand is  rax
- int [1-rst] - makes interrupt by number [1-rst]
- syscall - makes sycall based on ABI of registers
- lock ACTIONS - lock prefix to says CPU: next command will execute in atomic mode
- cltq - sign-extends [[registers|eax]] into [[registers|rax]]. This mnemonic is short form of  ==movslq %eax, %rax==. It copies the sign bit of [[registers|eax]] to all the upper bits of [[registers|rax]]