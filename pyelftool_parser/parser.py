import re
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
            for i in md.disasm(ops, addr):
                #print(i)
                self.inst[i.address] = (i.mnemonic, i.op_str)      
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
                    #print(hex(symbol['st_value']), symbol.name)
        f.close()

class parser:
    def __init__(self, symbol, inst):
        self.symbol = symbol 
        self.inst = inst
        self.stacklist = []


    def regexsub(self, inst, stacksize):    
        ## try to catch sub instruction for rbp,ebp,rsp,esp.
        searchrbp = re.search("rbp", inst[1])
        searchebp = re.search("ebp", inst[1])
        searchrsp = re.search("rsp", inst[1])
        searchesp = re.search("esp", inst[1])
        searchsub = re.match("sub", inst[0])
        if (searchrbp or searchebp or searchrsp or searchesp) and searchsub:
            dst = inst[1].split(",")[0]
            src = inst[1].split(",")[1]
            searchrsp = re.match("rsp", dst)
            if searchrsp:
                val = src
                print(int(val, 16))
                print(stacksize)
                stacksize = max(stacksize, int(val, 16))
                
        return stacksize
    def regexmov(self, inst, stacksize):
        ## check move rbp/ebp as dest,
        searchrbp = re.search("rbp", inst[1])
        searchebp = re.search("ebp", inst[1])
        searchrsp = re.search("rsp", inst[1])
        searchesp = re.search("esp", inst[1])
        searchmov = re.match("mov", inst[0])
        ## try to catch mov instruction and rbp r.
        if (searchrbp or searchebp or searchrsp or searchesp) and searchmov: 
            dst = inst[1].split(",")[0]
            src = inst[1].split(",")[1]
            searchminus = re.search("-", dst)
            if searchminus:
                val = dst.split("-")[1].replace("]","")
                stacksize = max(stacksize,int(val, 16))
        return stacksize

    def stack_analyzer(self):
        stacksize = 0
        for i in self.inst:
            if i in self.symbol.keys():  ## check function block (as basic block but we use function as unit.)
                #print(self.symbol[i])
                self.stacklist.append(stacksize)
                stacksize = 0
            print(stacksize)
            stacksize = max(stacksize, self.regexmov(self.inst[i], stacksize))
            print(stacksize)
            stacksize = max(stacksize, self.regexsub(self.inst[i], stacksize))
            print(stacksize)
        self.stacklist.append(stacksize)
        print(self.stacklist[1:])


                
if __name__ == '__main__':
    path = "../testbench/a.elf"
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    parser = parser(disassembler.symbol, disassembler.inst)
    parser.stack_analyzer()
