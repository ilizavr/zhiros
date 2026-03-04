
**base structure of the stack you are know, tell only about registers and details**

# a size of a stack
size of a stack can variabling,but in general **don't greater than 4 GBytes** because max size of a the segment is 4 GBytes,and stack storaging in **One** segment. upon flat model,stack can placed anywhere

# Constrains of the stack

- Max 4GB per stack segment in protected mode
    
- Stack grows downward toward lower addresses
    
- Current stack defined by SS:ESP pairing
    

# PUSH Operation

1. Decrement ESP by operand size (4/8 bytes)
    
2. Store operand at [SS:ESP]
    

- Stack grows toward lower addresses
    

# POP Operation

1. Read operand from [SS:ESP]
    
2. Increment ESP by operand size
    

- Stack shrinks toward higher addresses
    

# Return Instruction Pointer (RIP/EIP)

- CALL pushes return address (next instruction after CALL)
    
- RET pops return address into RIP/EIP
    
- Far RET additionally pops CS segment
    

# Stack Frame

- Memory region between EBP and ESP
    
- Contains: return address, parameters, saved registers, local variables
    
- EBP points to frame base, ESP to top
    

# Procedure

- Named block of code referenced by CALL
    
- Accessed via label, manages own stack frame
    
- Ends with RET to resume caller
    

# Near Call Algorithm

1. Push EIP/RIP (return address)
    
2. Load EIP/RIP with target offset
    

- Same code segment, uses current CS
    

# Far Call Algorithm
1. Push CS then push EIP/RIP
    
2. Load CS with new segment, EIP/RIP with target offset
    

- Different code segment, changes CS
    

# Near RET Algorithm

1. Pop return address into EIP/RIP
    
2. Resume execution at return address
    

# Far RET Algorithm

1. Pop return address into EIP/RIP
    
2. Pop segment into CS
    
3. Resume execution at return address in new segment
    

# Stack Usage
- Single stack active per thread (SS:ESP)
    
- Multiple frames coexist via nesting
    
- Frame created on CALL, destroyed on RET