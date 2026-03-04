
# lexer
**Definition:** you are know for why purpose the part - for deliming code on tokens for further  comfortable  representing it on AST

### how is it works
when lexer analys seeing a char, he defines type of char in lexer. this types and corresponds values:
- ID - is token that represents  name of variable,function, in generally name of something that  storaging in memory
- NUM - it's any numeric constant
- STATEMENT_TOKENS - it's tokens that corresponds something language statement  or keyword.  such as =,;,if,while
- LITERAL - any string character  beetwen " "

# symbol-table manager
the table is array that contains all supported keywords in compiler. for why? in 4.11 moment IDK, but tommorow obtain knowlege