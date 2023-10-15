import register
from capstone import *
from capstone.x86 import *
from debug import log
class execute:
    def __init__(self, register):
        self.register = register
        self.reg = register.reg
    def exe(self, inst):
        ## -----------------------------------------------
        ## decode stage.

        (regs_read, regs_write) = inst.regs_access()
        ##  catch the rsp reg in instruction.    
        flagrsp = 0
        src =0
        dst = 0
        reg = []
        readreg = []
        writereg = []
        flagimm = 0
        flagmem = 0
        flagptr = 0
        imm = 0
        disp = 0
        memindex = 0
        if "ptr" in inst.op_str:    ## early exit for ptr, I do not handle the pointer to memory yet.
            ##log(inst.address, inst.mnemonic, inst.op_str)
            ##log("I do not handle memory yet")
            return 0
        
        if len(inst.operands) >= 2:
            src = inst.op_str.split(",")[1].replace(" ","")
            dst = inst.op_str.split(",")[0]
        elif len(inst.operands) == 1:
            dst = inst.op_str
        for i in inst.operands:
            if i.type == X86_OP_REG:
                reg.append(inst.reg_name(i.reg))
            if i.type == X86_OP_IMM:
                imm = i.imm
                flagimm = 1
            if i.type == X86_OP_MEM:
                base = inst.reg_name(i.mem.base)
                disp = i.mem.disp
                memindex = i.mem.index
                flagmem = 1
        for r in regs_read:  ## catch the implicity read register of stack
            readreg.append(inst.reg_name(r))
            if "rsp" in inst.reg_name(r):
                flagrsp = 1
        for r in regs_write: ## catch the implicity write register of stack
            writereg.append(inst.reg_name(r))
            if "rsp" in inst.reg_name(r):
                flagrsp = 1
        
        ##------------------------------------------
        ## execute stage.

        if flagrsp:  ## if rsp is in the instruction
            if inst.id == (X86_INS_PUSH):  ## catch push
                self.reg["rsp"] -= 4
            elif inst.id == (X86_INS_POP): ## catch pop instruction
                self.reg["rsp"] += 4
            elif inst.id == (X86_INS_MOV):  ## catch mov instruction
                if flagimm:
                    self.reg[dst] = imm
                else:
                    self.reg[dst] = self.reg[src]
            elif inst.id == (X86_INS_SUB):  ## catch sub instruction
                if flagimm:
                    self.reg[dst] -= imm
                else:
                    self.reg[dst] -= self.reg[src]
                
            elif inst.id == (X86_INS_ADD):  ## catch add instruction
                if flagimm:
                    self.reg[dst] += imm
                else:
                    self.reg[dst] += self.reg[src]
            elif inst.id == (X86_INS_LEA):  ## catch lea instruction
                log("LEA instruction have not yet handled")
            elif inst.id == (X86_INS_CALL):  ## catch call instruction
                self.reg["rsp"] -= 4
            elif inst.id == (X86_INS_RET):  ## catch RET instruction
                self.reg["rsp"] += 4
            else:
                log(inst.address, inst.mnemonic, inst.op_str)
                log("we have not catched this instruction")
                return 0
        else:
            log(inst.address, inst.mnemonic, inst.op_str)
            log("It is not about rsp")