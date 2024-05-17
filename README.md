# Prototype Parser to Analyse bytecode.
## The goal of this parser is to evaluate how much stack size we need in the program from bytecode.
    In this prototype, I use "objdump -s target.elf"/ "target.s" as input to my parser.
    And I assume the format is Intel format in the assembly code. And I use capstone library to help me parse the assembly code.
    In the first phase of parser, I try to catch the move instruction to dst "rbp,ebp".

### How to use.
    Set up the path to target.s in analyzer.py main function. 

### TODO
        1. Handle the recursive function, and runtime function call. We decide to detect it and report as an exception.
        2. Going to implement the tiny instruction set simulator to see how much information we could get in static analysis.

