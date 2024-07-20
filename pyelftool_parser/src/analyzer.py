import register
import execute
from debug import loginst, log, logresult
from capstone.x86 import *
from elftools.elf.elffile import ELFFile
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
        self.entry_pc = 0
        self.exit_pc = 0

    def disasminst(self):
        pc_flag = 0
        with open(self.path, 'rb') as f:
            elf = ELFFile(f)
            code = elf.get_section_by_name('.text')
            ops = code.data()
            addr = code['sh_addr']
            md = Cs(CS_ARCH_X86, CS_MODE_64)
            md.detail = True
            for i in md.disasm(ops, addr):
                self.inst[i.address] = (i)
                if (i.address in self.symbol):  ## it is to catch the next symbol start. @minghwu: I think it could have a better way to do this.
                    pc_flag = 0
                if (i.address == self.entry_pc): ## when it is entry point, set the flag = 1 to catch exitpoint.
                    pc_flag = 1
                if (pc_flag == 1):               ## catch the point until the next symbol, means it is exit point.
                    self.exit_pc = i.address
                log(f'0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}')

    def disasmsymbol(self):
        with open(self.path, 'rb') as f:
            e = ELFFile(f)
            symbol_tables = [ s for s in e.iter_sections()
                         if isinstance(s, SymbolTableSection)]
            for section in symbol_tables:
                for symbol in section.iter_symbols():
                    self.symbol[symbol['st_value']] = symbol.name
                    self.vertex[symbol['st_value']] = symbol.name
                    log("here")
                    log(symbol.name, symbol['st_value'])
                    if(symbol.name == '__cosrt_upcall_entry'): ## set up the entry pc.
                        self.entry_pc = symbol['st_value']
                        log("Set up entry point")
                        log(hex(self.entry_pc))
    def sym_analyzer(self):
        sym_info = {}
        with open(self.path, 'rb') as f:
            e = ELFFile(f)
            symbol_tables = [ s for s in e.iter_sections()
                         if isinstance(s, SymbolTableSection)]
            for section in symbol_tables:
                for symbol in section.iter_symbols():
                    if (symbol['st_size'] == 0):
                        continue   
                    sym_info[symbol.name] = {
                        'address': symbol['st_value'],
                        'size': symbol['st_size'],
                        'padding': 0   
                    }
            #sort by adress to ensure contiguous symbols
            sorted_names = sorted(sym_info.keys(),
            key=lambda name: sym_info[name]['address'],
            reverse=False)
            #assign padding based on difference in address
            for i, name in enumerate(sorted_names):
                if (i == 0):
                    continue
                prev_sym = sym_info[sorted_names[i - 1]]
                cur_sym = sym_info[name]
                prev_sym['padding'] = cur_sym['address'] - prev_sym['address'] - prev_sym['size']
                #sort by size
            sorted_names = sorted(sym_info.keys(),key=lambda name: sym_info[name]['size'], reverse=True)
            #print symbols in order of size with padding
            for name in sorted_names[:10]:
                cur_sym = sym_info[name]
                print(
                    f"Name: {name}, Address: {hex(cur_sym['address'])}, Size: {hex(cur_sym['size'])}, Padding: {hex(cur_sym['padding'])}"
                )
    
class parser:
    def __init__(self, symbol, inst, register, execute, exit_pc):
        self.symbol = symbol 
        self.inst = inst
        self.stacklist = []
        self.stackfunction = []
        self.register = register
        self.execute = execute
        self.edge = set()
        self.vertex = set()
        self.index = 0
        self.exit_pc = exit_pc
        self.retjmppc = 0
        self.retjmpflag = 0
        self.retcallpc = 0
        self.seenlist = [] ## handle the while loop jmp.
    def stack_analyzer(self):
        index_list = list(self.inst.keys())
        index_list.append(-1) ## dummy value for last iteration.
        self.index = index_list.index(self.register.reg["pc"])
        nextinstRip = list(self.inst.keys())
        nextinstRip.append(-1) ## dummy value for last iteration.
        while(self.register.reg["pc"] != self.exit_pc):
            print(nextinstRip[self.index + 1 if self.index + 1 in nextinstRip else self.index])
            self.register.updaterip(nextinstRip[self.index + 1 if self.index + 1 in nextinstRip else self.index]) ## catch the rip for memory instruction.
            if self.register.reg["pc"] in self.symbol.keys():  ## check function block (as basic block but we use function as unit.)
                self.stackfunction.append(self.symbol[self.register.reg["pc"]])
                self.stacklist.append(self.register.reg["stack"])
                self.register.clean()
                ###### Graph
                vertexfrom = self.register.reg["pc"]
                self.vertex.add(vertexfrom)
            self.execute.exe(self.inst[self.register.reg["pc"]], self.edge, vertexfrom)
            ## logresult(self.register.reg["stack"], hex(key))
            self.register.updatestackreg()
            
            #### set up next instruction pc
           
            if (self.index == index_list.index(self.register.reg["pc"])):  ## fetch next instruction
                if self.inst[self.register.reg["pc"]].id == (X86_INS_RET): ## encounter ret instruction to set pc
                    self.index = index_list.index(self.retcallpc)
                elif index_list[self.index + 1] in self.symbol.keys() and self.retjmpflag == 1: ## handle the return if there is no ret.
                    self.index = index_list.index(self.retjmppc)
                    self.retjmpflag = 0
                else:
                    self.index = self.index + 1
            else:     ## handle the call and jmp instruction
                if self.inst[index_list[self.index]].id == (X86_INS_CALL): ## here is error. handle ret
                    self.retcallpc = index_list[self.index + 1]
                    self.index = index_list.index(self.register.reg["pc"])
                else:  ## handle the while jmp.
                    self.retjmppc = index_list[self.index + 1]  ## set the return point
                    self.retjmpflag = 1
                    if self.register.reg["pc"] not in self.seenlist:
                        self.index = index_list.index(self.register.reg["pc"])
                        self.seenlist.append(self.register.reg["pc"])
                    else:
                        self.index = self.index + 1
            ####
            self.register.reg["pc"] = index_list[self.index]
            log(self.index)
            log(hex(self.register.reg["pc"]))
        ## self.stacklist.append(self.register.reg["stack"])
        ## self.stacklist = self.stacklist[1:]
        ## logresult(self.stackfunction)
        ## logresult(self.stacklist)
        return (self.stackfunction,self.stacklist)
    

def driver(disassembler, register, execute, parser):
    disassembler.disasmsymbol()
    disassembler.disasminst()
    disassembler.sym_analyzer()
    parser.stack_analyzer()
    logresult(parser.edge)
    return parser.stacklist


if __name__ == '__main__':
    #path = "../testbench/selftest/a.elf"
    #path = "../usr/bin/gcc"
    #path = "../testbench/dhrystone/dhrystone"
    #path = "../testbench/composite/tests.unit_pingpong.global.ping"
    path = "../testbench/composite/system_binaries/cos_build-test/global.sched/sched.pfprr_quantum_static.global.sched"
    
    
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    log("entry:"+ str(disassembler.entry_pc))
    log("entry:"+ str(disassembler.exit_pc))
    register = register.register()
    register.reg["pc"] = disassembler.entry_pc
    execute = execute.execute(register)
    parser = parser(disassembler.symbol, disassembler.inst, register, execute, disassembler.exit_pc)
    driver(disassembler, register, execute, parser)
    log(parser.edge)
    
    
