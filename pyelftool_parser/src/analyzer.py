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
        self.vertex = dict()

    def disasminst(self):
        with open(self.path, 'rb') as f:
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
            #f.close()

    def disasmsymbol(self):
        with open(self.path, 'rb') as f:
            e = ELFFile(f)
            symbol_tables = [ s for s in e.iter_sections()
                         if isinstance(s, SymbolTableSection)]
            for section in symbol_tables:
                for symbol in section.iter_symbols():
                    self.symbol[symbol['st_value']] = symbol.name
                    self.vertex[symbol['st_value']] = symbol.name                  
                    #print(symbol.name)
            #f.close()

class parser:
    def __init__(self, symbol, inst, register, execute):
        self.symbol = symbol 
        self.inst = inst
        self.stacklist = []
        self.stackfunction = []
        self.register = register
        self.execute = execute
        self.edge = set()
        self.vertex = set()
    def stack_analyzer(self):
        nextinstkey = list(self.inst.keys())
        nextinstkey.append(-1) ## dummy value for last iteration.
        index = 0
        for key in self.inst.keys():
            self.register.reg["pc"] = key
            self.register.updaterip(nextinstkey[index + 1]) ## catch the rip for memory instruction.
            index = index + 1
            if key in self.symbol.keys():  ## check function block (as basic block but we use function as unit.)
                self.stackfunction.append(self.symbol[key])
                self.stacklist.append(self.register.reg["stack"])
                #print(self.symbol[key])
                #print(register.reg["stack"])
                #print()
                self.register.clean()
                ###### Graph
                vertexfrom = key
                self.vertex.add(vertexfrom)
            self.execute.exe(self.inst[key],self.edge,vertexfrom)
            self.register.updatestackreg()

        self.stacklist.append(self.register.reg["stack"])
        self.stacklist = self.stacklist[1:]
        print(self.stackfunction)
        print(self.stacklist)
        return (self.stackfunction,self.stacklist)

def driver(disassembler, register, execute, parser):
    path = "../testbench/selftest/a.elf"
    #path = "../usr/bin/gcc"
    #path = "../testbench/dhrystone/dhrystone"
    disassembler.disasmsymbol()
    disassembler.disasminst()
    
    parser.stack_analyzer()
    print(parser.edge)
    return parser.stacklist;


if __name__ == '__main__':
    path = "../testbench/selftest/a.elf"
    #path = "../usr/bin/gcc"
    #path = "../testbench/dhrystone/dhrystone"
    
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()

    register = register.register()
    execute = execute.execute(register)
    parser = parser(disassembler.symbol, disassembler.inst, register, execute)
    driver(disassembler, register, execute, parser)
    print(parser.edge)
    
    
