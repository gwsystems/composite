import re
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from capstone import *

class disassembler:
    def __init__(self, path):
        self.path = path
        self.dict = dict()
    def disasminst(self):
        with open(path, 'rb') as f:
            elf = ELFFile(f)
            code = elf.get_section_by_name('.text')
            ops = code.data()
            addr = code['sh_addr']
            md = Cs(CS_ARCH_X86, CS_MODE_64)
            for i in md.disasm(ops, addr):
                #print(i)
                self.dict[i.address] = (i.mnemonic, i.op_str)      
                #print(f'0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}')
            #print(self.dict)

    def disasmsymbol(self):
        with open(self.path, 'rb') as f:
            e = ELFFile(f)
            code = e.get_section_by_name('.text')
            for i in code.iter_relocations():
                print(i)

            for section in e.iter_sections():
                if isinstance(section, RelocationSection):
                    #print(f'{section.name}:')
                    symbol_table = e.get_section(section['sh_link'])
                    for relocation in section.iter_relocations():
                        symbol = symbol_table.get_symbol(relocation['r_info_sym'])
                        addr = hex(relocation['r_offset'])
                    #    print(f'{symbol.name} {addr}')
class parser:
    def __init__(self, path):
        self.header = []
        self.stack = []
        self.recursive = []
        self.path = path
    def parseheader(self):
        with open(self.path) as f:
            contents = f.readlines()
        for data in contents:
            start = re.search("<*>:", data)
            if start:
                function_name = data.strip().split("<")[1].replace(">:","")
                self.header.append(function_name)
        f.close()

    def regexmov(self, inst, stacksize):
        ## check move rbp/ebp as dest, 
        searchrbp = re.search("rbp", inst)
        searchebp = re.search("ebp", inst)
        searchminus = re.search("-", inst)
        searchmov = re.match("mov", inst)
        ## try to catch mov instruction.
        if (searchrbp or searchebp) and searchminus and searchmov: 
            searchrbp = re.search("rbp", inst.split(",")[-1])  ## search the destination is rbp and ebp, dst in in the last index split string.
            searchebp = re.search("ebp", inst.split(",")[-1])
            if (searchrbp or searchebp):
                val = inst.split(",")[-1].split("(")[0].replace("-", "")    ## get dst of inst. For example, mov    %r12b,-0x1(%rbp)   
                if (re.match("0x*", val)):
                    size = int(val, 16)
                    stacksize = max(stacksize, size)
                else:
                    val = inst.split(" ")[-1].split(",")[0].split("(")[0].replace("-", "")   ## get the source of inst. For example, movzbl -0xc(%rdx),%ebp
                    size = int(val, 16)
                    stacksize = max(stacksize, size)
        return stacksize

    def regexsub(self, inst, stacksize):    
        ## try to catch sub instruction for rsp.
        searchrbp = re.search("rbp", inst)
        searchebp = re.search("ebp", inst)
        searchrsp = re.search("rsp", inst)
        searchsub = re.match("sub", inst)
        if (searchrbp or searchebp or searchrsp) and searchsub: 
            searchrbp = re.search("rbp", inst.split(",")[-1])  ## search the destination is rbp and ebp, dst in in the last index split string.
            searchebp = re.search("ebp", inst.split(",")[-1])
            searchrsp = re.search("rsp", inst.split(",")[-1])
            if (searchrbp or searchebp or searchrsp):
                temp = []
                for i in inst.split(" "):  ## remove the blank
                    if len(i)!=0:
                        temp.append(i)
                instruction = temp
                dst = instruction[1].split(",")[1]
                src = instruction[1].split(",")[0]
                searchexception1 = re.search("\(", dst)
                searchexception2 = re.search("\(", src)
                if ( not searchexception1 and not searchexception2):
                    if (re.match("\$0x*", src)):
                        size = int(src.replace("$",""), 16)
                        stacksize = max(stacksize, size)
        return stacksize
    def regexpush(self, inst, push_count, push_maxcount):
        ## try to catch push instruction for rsp.
        searchpush = re.match("push", inst)
        searchpop = re.match("pop", inst)
        if searchpush:
            push_count += 1
            push_maxcount = max(push_count, push_maxcount)
        if searchpop:
            push_count -= 1
        return push_maxcount

    def stack_analyzer(self):
        with open(self.path) as f:
            contents = f.readlines()
        stacksize = 0
        push_count = 0
        push_maxcount = 0
        stacklist = []
        headlist = []

        for data in contents:
            start = re.search("<*>:", data.strip())
            if start: ## catch the symbol.
                stacklist.append(max(stacksize,push_maxcount << 2))
                headlist.append(data.strip())
                stacksize = 0
                push_count = 0
                push_maxcount = 0
                continue
            temp = data.strip().split("\t")
            for inst in temp:
                stacksize = max(stacksize, self.regexmov(inst, stacksize)) ## catch mov instruction
                stacksize = max(stacksize, self.regexsub(inst, stacksize)) ## catch sub instruction
                push_maxcount = self.regexpush(inst, push_count, push_maxcount) ## catch push instruction

        stacklist.append(stacksize) ## catch the last loop.
        stacklist = stacklist[1:]  ## skip the first loop.
        self.stack = stacklist
        f.close()

    def parserecurrence(self):
        stacklist = []
        headlist = []
        with open(self.path) as f:
            contents = f.readlines()
        function_name = ""
        for data in contents:
            start = re.search("<*>:", data.strip())
            if start:
                function_name = data.strip().split("<")[1].replace(">:","")
                self.recursive.append("No")
            temp = data.strip().split("\t")
            for inst in temp:            
                searchcall = re.match("call", inst)
                if (searchcall):
                    searchrecursive = re.search("<" + function_name + ">", inst)
                    if (searchrecursive):
                        self.recursive[-1] = "Yes"
        f.close()  
                
if __name__ == '__main__':
    path = "../testbench/a.elf"
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    #parser = parser(path)
    #parser.parseheader()
    #parser.stack_analyzer()
    #parser.parserecurrence()
    #print(parser.header)
    #print(parser.stack)
    #print(parser.recursive)