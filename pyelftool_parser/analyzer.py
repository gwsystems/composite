import register
import re
import execute
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from capstone import *
from elftools.elf.sections import (
    NoteSection, SymbolTableSection, SymbolTableIndexSection
)
class disassembler:
    def __init__(self, path):
        self.path = path
        self.inst = dict()
        self.symbol = dict()

    def disasminst(self):
        with open(path, 'rb') as f:
            elf = ELFFile(f)
            code = elf.get_section_by_name('.text')
            ops = code.data()
            addr = code['sh_addr']
            md = Cs(CS_ARCH_X86, CS_MODE_64)
            md.detail = True
            for i in md.disasm(ops, addr):
                #print(i)
                self.inst[i.address] = (i)      
                #print(f'0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}')
        f.close()

    def disasmsymbol(self):
        with open(self.path, 'rb') as f:
            e = ELFFile(f)
            symbol_tables = [(idx, s) for idx, s in enumerate(e.iter_sections())
                         if isinstance(s, SymbolTableSection)]
            for section_index, section in symbol_tables:
                for nsym, symbol in enumerate(section.iter_symbols()):
                    self.symbol[symbol['st_value']] = symbol.name
        f.close()

class parser:
    def __init__(self, symbol, inst, register, execute):
        self.symbol = symbol 
        self.inst = inst
        self.stacklist = []
        self.stackfunction = []
        self.register = register
        self.execute = execute

    def stack_analyzer(self):
        stacksize = 0
        nextinstkey = list(self.inst.keys())
        nextinstkey.append(-1) ## dummy value for last iteration.
        index = 0
        for key in self.inst.keys():
            register.reg["pc"] = key
            register.updaterip(nextinstkey[index + 1])
            index = index + 1
            
            if key in self.symbol.keys():  ## check function block (as basic block but we use function as unit.)
                self.stackfunction.append(self.symbol[key])
                self.stacklist.append(stacksize)
                stacksize = 0
            execute.exe(self.inst[key])


                
if __name__ == '__main__':
    path = "../testbench/a.elf"
    #path = "/usr/bin/gcc"
    
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    register = register.register()
    execute = execute.execute(register)

    parser = parser(disassembler.symbol, disassembler.inst, register, execute)
    parser.stack_analyzer()
    #print(parser.stackfunction)
    #print(parser.stacklist)
