- **ESP/RSP**: Stack pointer - points to top of stack
    
- **EBP/RBP**: Base pointer - points to current stack frame base
    
- **SS**: Stack segment selector (flat model: base=0, limit=4GB)


 - the segment registers (CS, DS, SS, ES, FS, and GS) hold 16-bit segment selectors. A segment selector is a special
pointer that identifies a segment in memory.


when in os using flat memory model, segment registers are working like as:
- all segment selectors points to overlap segments each points to 0
- the overlap segments comprise all linear memory space of a proggram
- but typaclly, two important segments are defined: one for code, and second for data or stack 

when in os using segment memory model,segment registers are working like as:
- every segment register points to corresponds segment in linear addres
- if proggram wish to accses segment not pointed by segment registers,proggram must load segment selector for correspond segment  to obtain an accses to segment
