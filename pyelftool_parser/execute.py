import register
from capstone import *
from capstone.x86 import *
from debug import log
class execute:
    def __init__(self, register):
        self.register = register
        self.reg = register.reg
    def exe(self, inst):

        (regs_read, regs_write) = inst.regs_access()
        ##  catch the rsp reg in instruction.    
        flag = 0
        src = []
        dst = []
        log(inst.address, inst.mnemonic, inst.op_str)
        log(inst.operands)
        flagimm = 0
        flagmem = 0
        imm = 0
        disp = 0
        for i in inst.operands:
            if i.type == X86_OP_REG:
                log("cccc")
                log(inst.reg_name(i.reg))
            if i.type == X86_OP_IMM:
                imm = i.imm
                flagimm = 1
            if i.type == X86_OP_MEM:
                base = inst.reg_name(i.mem.base)
                disp = i.mem.disp
                flagmem = 1
        for r in regs_read:  ## catch the implicity read register of stack
            src.append(inst.reg_name(r))
            log("bbbbb")
            log(src)
            if "rsp" in inst.reg_name(r):
                flag = 1
        for r in regs_write: ## catch the implicity write register of stack
            dst.append(inst.reg_name(r))
            log("aaaaa")
            log(dst)
            if "rsp" in inst.reg_name(r):
                flag = 1
    
        if flag:  ## if rsp is in the instruction
            if inst.id == (X86_INS_PUSH):  ## catch push
                self.reg["rsp"] -= 4
            elif inst.id == (X86_INS_POP):
                self.reg["rsp"] += 4
            elif inst.id == (X86_INS_MOV):  ## catch mov instruction
                if len(dst) == 1:
                    self.reg[dst[0]] = self.reg[src[0]]
                else:
                    log("we have not catched this instruction")
            elif inst.id == (X86_INS_SUB):
                if len(dst) == 1:   ## catch the immediate
                    self.reg[dst[0]] = int(src[0], 16)
                else:
                    log("we have not catched this instruction")
            elif inst.id == (X86_INS_ADD):
                i = 1
            elif inst.id == (X86_INS_LEA):
                i = 1
            else:
                log(inst.address, inst.mnemonic, inst.op_str)
                log("we have not catched this instruction")
        