from capstone import *
from capstone.x86 import *
from debug import loginst,log, logcall, logerror
class execute:
    def __init__(self, register):
        self.register = register
        self.reg = register.reg
        self.retflag = 0
    def exe(self, inst, edge, vertexfrom):
        ## -----------------------------------------------
        (regs_read, regs_write) = inst.regs_access()
        ##  catch the rsp reg in instruction.    
        flagrsp = 0
        src = 0
        dst = 0
        reg = []
        readreg = []
        writereg = []
        flagimm = 0
        flagmem = 0
        imm = 0
        disp = 0
        self.retflag = 0
        ############# decode stage.
        loginst("instruction")
        loginst(hex(inst.address), inst.mnemonic, inst.op_str)
        if "ptr" in inst.op_str:    ## early exit for ptr, I do not handle the pointer to memory yet.
            loginst(inst.address, inst.mnemonic, inst.op_str)
            loginst("I do not handle ptr memory yet")
            return 0
        if len(inst.operands) >= 2:
            if (inst.id == X86_INS_FXCH):
                dst = inst.op_str
                log(dst)
            elif (inst.id == X86_INS_LEA):
                src = inst.op_str.split(",")[1].replace("[","").replace("]","")
                ## This eval might be buggy because I assume we have only bit 64 register in the ptr for LEA instruction.
                src = eval(src,self.reg)
                dst = inst.op_str.split(",")[0]
            else:
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
                flagmem = 1
        for r in regs_read:  ## catch the implicity read register of stack
            readreg.append(inst.reg_name(r))
            if "rsp" in inst.reg_name(r):
                flagrsp = 1
        for r in regs_write: ## catch the implicity write register of stack
            writereg.append(inst.reg_name(r))
            if "rsp" in inst.reg_name(r):
                flagrsp = 1
        ############################################
        ##------------------------------------------
        ## execute stage.
        if flagrsp:  ## if rsp is in the instruction
            if inst.id == (X86_INS_PUSH):  ## catch push
                self.reg["rsp"] -= 8
            elif inst.id == (X86_INS_POP): ## catch pop instruction
                self.reg["rsp"] += 8
            elif inst.id == (X86_INS_MOV):  ## catch mov instruction
                if flagimm:
                    self.register.Setreg(dst, imm)
                else:
                    self.register.Setregwithregname(dst, src)
            elif inst.id == (X86_INS_MOVABS):  ## catch mov instruction
                if flagimm:
                    self.register.Setreg(dst, imm)
                else:
                    self.register.Setregwithregname(dst, src)
            elif inst.id == (X86_INS_SUB):  ## catch sub instruction
                if flagimm:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) - imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) - self.register.Getregwithname(src))
            elif inst.id == (X86_INS_AND):  ## catch sub instruction
                if flagimm:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) & imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) & self.register.Getregwithname(src))
            elif inst.id == (X86_INS_ADD):  ## catch add instruction
                if flagimm:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) + imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) + self.register.Getregwithname(src))
            elif inst.id == (X86_INS_LEA):  ## catch lea instruction
                self.reg[dst] = src
            elif inst.id == (X86_INS_CALL):  ## catch call instruction
                self.reg["rsp"] -= 8
                if (not flagimm):
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static call") 
                    self.reg["pc"] = int(dst, 0)  ## hex to int
                    edge.add((hex(vertexfrom), hex(imm)))  ## graph
            elif inst.id == (X86_INS_RET):  ## catch RET instruction
                self.reg["rsp"] += 8
                self.retflag = 1
                
            elif inst.id == (X86_INS_ENTER):  ## catch enter instruction, here is a problem, I am not sure how much the imm.
                logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                self.reg["rsp"] -= imm
                self.reg["enter"] = imm
                ## TODO: here is problematic, We need a stack to store what is inside the 
            elif inst.id == (X86_INS_LEAVE):  ## catch Leave instruction
                logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                self.reg["rsp"] += self.reg["enter"]

            else:
                loginst(hex(inst.address), inst.mnemonic, inst.op_str)
                loginst("we have not catched this instruction and it is rsp instruction.")
                return 0
            return 0
        else: ## simulator mode or calculation mode
            if inst.id == (X86_INS_MOV):  ## catch mov instruction
                if flagimm:
                    self.register.Setreg(dst, imm)
                else:
                    self.register.Setregwithregname(dst, src)
            elif inst.id == (X86_INS_MOVABS):  ## catch mov instruction
                if flagimm:
                    self.register.Setreg(dst, imm)
                else:
                    self.register.Setregwithregname(dst, src)
            elif inst.id == (X86_INS_MOVABS):  ## catch mov instruction
                if flagimm:
                    self.register.Setreg(dst, imm)
                else: 
                    self.register.Setregwithregname(dst, src)
            elif inst.id == (X86_INS_LEA):  ## catch mov instruction
                self.reg[dst] = src
            elif inst.id == (X86_INS_SUB):  ## catch sub instruction
                if flagimm:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) - imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) - self.register.Getregwithname(src))
            elif inst.id == (X86_INS_AND):  ## catch sub instruction
                if flagimm:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) & imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) & self.register.Getregwithname(src))
            elif inst.id == (X86_INS_ADD):  ## catch add instruction
                if flagimm:
                     self.register.Setreg(dst, self.register.Getregwithname(dst) + imm)
                else:
                    self.register.Setreg(dst, self.register.Getregwithname(dst) + self.register.Getregwithname(src))
            elif inst.id == (X86_INS_ENTER):
                loginst(hex(inst.address), inst.mnemonic, inst.op_str)
            elif inst.id == (X86_INS_JMP): ## NOT yet implemented in simulation machine
                if (not flagimm):
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static jump")
                    self.reg["pc"] = int(dst, 0)
                    edge.add((hex(vertexfrom), dst))  ## graph
            elif inst.id == (X86_INS_JE): ## NOT yet implemented in simulation machine
                if (not flagimm):
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static jump")
                    self.reg["pc"] = int(dst, 0)
                    edge.add((hex(vertexfrom), dst))  ## graph
            elif inst.id == (X86_INS_JLE): ## NOT yet implemented in simulation machine
                if (not flagimm):
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static jump")
                    self.reg["pc"] = int(dst, 0)
                    edge.add((hex(vertexfrom), dst))  ## graph
            elif inst.id == (X86_INS_JGE): ## NOT yet implemented in simulation machine
                if (not flagimm):
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static jump")
                    self.reg["pc"] = int(dst, 0)
                    edge.add((hex(vertexfrom), dst))  ## graph
            elif inst.id == (X86_INS_JG): ## NOT yet implemented in simulation machine
                if (not flagimm):
                    logerror("here is dynamic that we do not handle.")
                    logerror(hex(inst.address), inst.mnemonic, inst.op_str)
                else:
                    logcall(hex(inst.address), inst.mnemonic, inst.op_str)
                    logcall("here is an static jump")
                    self.reg["pc"] = int(dst, 0)
                    edge.add((hex(vertexfrom), dst))  ## graph
            else:
                loginst(hex(inst.address), inst.mnemonic, inst.op_str)
                loginst("This instruction is not yet handled in simulator mode which is not rsp instruction.")
            loginst(hex(inst.address), inst.mnemonic, inst.op_str)
            loginst("this instruction is not about rsp.")
            return 0