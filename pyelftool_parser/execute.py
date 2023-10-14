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
        for r in regs_read:
             if "rsp" in inst.reg_name(r):
                 log(inst.address, inst.mnemonic, inst.op_str)
                 flag = 1
        for r in regs_write:
            if "rsp" in inst.reg_name(r):
                 log(inst.address, inst.mnemonic, inst.op_str)
                 flag = 1
        if flag:  ## if rsp is in the instruction
            if inst.id == (X86_INS_PUSH):  ## catch push
                log(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_POP):
                log(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_MOV):  ## catch mov instruction
                log(inst.address, inst.mnemonic, inst.op_str)
                dst = inst.op_str
            elif inst.id == (X86_INS_SUB):
                log(inst.address, inst.mnemonic, inst.op_str)
                
            elif inst.id == (X86_INS_ADD):
                log(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_LEA):
                log(inst.address, inst.mnemonic, inst.op_str)
            else:
                log("we have not catch this instruction")
                log(inst.address, inst.mnemonic, inst.op_str)
        