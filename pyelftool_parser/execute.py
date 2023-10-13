import register
from capstone import *
from capstone.x86 import *
class execute:
    def __init__(self, register):
        self.register = register
        self.reg = register.reg
    def exe(self, inst):

        if inst.id == (X86_INS_PUSH):  ## catch push
            print(inst.address, inst.mnemonic, inst.op_str)
        elif inst.id == (X86_INS_PUSH):
            print(inst.address, inst.mnemonic, inst.op_str)
        elif "rsp" not in inst.op_str:
            return 0  ## early exit
        else:
            if inst.id == (X86_INS_MOV):  ## catch mov instruction
                print(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_SUB):
                print(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_ADD):
                print(inst.address, inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_LEA):
                print(inst.address, inst.mnemonic, inst.op_str)
            else:
                print("we have not catch this instruction")
                print(inst.address, inst.mnemonic, inst.op_str)

