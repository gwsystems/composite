# Prototype Parser to Analyse bytecode.
## The goal of this parser is to evaluate how much stack size we need in the program from bytecode.
    In this prototype, I use "objdump -s target.elf"/ "target.s" as input to my parser.
    And I assume the format is AT&T format in the assembly code.
    In the first phase of parser, I try to catch the move instruction to dst "rbp,ebp".

### How to use.
    Set up the path to target.s in parser.py main function. 

### TODO
        1. catch the sub instruction of the rbp/ebp.
        2. might need to catch the immediate of address. For example, mov -0x4(rax + rbx + 8), rbp
        3. parse the recursion function.
        4. Format the output.

