from debug import log, logstack
class register:
    def __init__(self):
        self.reglist = ["rax", "rbx", "rcx", "rdx", "rdi", "rsi", "rbp", "rsp", 
                        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", 
                        "eax", "ebx", "ecx", "edx", "edi", "esi", "ebp", "esp",
                        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"]
        self.reg = dict()
        self.reg["pc"] = 0
        self.reg["rax"] = 0
        self.reg["rbx"] = 0
        self.reg["rcx"] = 0
        self.reg["rdx"] = 0
        self.reg["rdi"] = 0
        self.reg["rsi"] = 0
        self.reg["rbp"] = 0
        self.reg["rspbegin"] = 0
        self.reg["rsp"] = 0
        self.reg["r8"] = 0
        self.reg["r9"] = 0
        self.reg["r10"] = 0
        self.reg["r11"] = 0
        self.reg["r12"] = 0
        self.reg["r13"] = 0
        self.reg["r14"] = 0
        self.reg["r15"] = 0
        self.reg["xmm0"] = 0
        self.reg["xmm1"] = 0
        self.reg["xmm2"] = 0
        self.reg["xmm3"] = 0
        self.reg["xmm4"] = 0
        self.reg["xmm5"] = 0
        self.reg["xmm6"] = 0
        self.reg["xmm7"] = 0
        self.reg["enter"] = 0
        self.reg["stack"] = 0
    def clean(self):
        self.reg["rax"] = 0
        self.reg["rbx"] = 0
        self.reg["rcx"] = 0
        self.reg["rdx"] = 0
        self.reg["rdi"] = 0 
        self.reg["rsi"] = 0
        self.reg["rbp"] = 0
        self.reg["rspbegin"] = self.reg["rsp"]
        self.reg["rsp"] = 0
        self.reg["r8"] = 0
        self.reg["r9"] = 0
        self.reg["r10"] = 0
        self.reg["r11"] = 0
        self.reg["r12"] = 0
        self.reg["r13"] = 0
        self.reg["r14"] = 0
        self.reg["r15"] = 0
        self.reg["xmm0"] = 0
        self.reg["xmm1"] = 0
        self.reg["xmm2"] = 0
        self.reg["xmm3"] = 0
        self.reg["xmm4"] = 0
        self.reg["xmm5"] = 0
        self.reg["xmm6"] = 0
        self.reg["xmm7"] = 0
        self.reg["enter"] = 0
        self.reg["stack"] = 0
    def Isreg(self, s):
        if s in self.reglist:
            return True
        else:
            return False
    def Getregwithname(self, s):
        if s == "al" or s == "bl" or s == "cl" or s == "dl":
            return (self.reg["r" + s.replace("l","x")] & 0xff)
        elif s == "ah" or s == "bh" or s == "ch" or s == "dh":
            return (( self.reg["r" + s.replace("h","x")] & 0xff00) >> 8)
        elif s == "ax" or s == "bx" or s == "cx" or s == "dx" or s == "bp" or s == "si" or s == "di" or s == "sp":
            return (self.reg["r" + s] & 0xffff)
        elif s == "eax" or s == "ebx" or s == "ecx" or s == "edx" or s == "edi" or s == "esi" or s == "ebp" or s == "esp":
            return self.reg[s.replace("e","r")] & 0xffffffff
        elif ("r8" in s) or ("r9" in s) or ("r10" in s) or ("r11" in s) or ("r12" in s) or ("r13" in s) or ("r14" in s) or (("r15" in s)):
            if s[-1] == "d":
                return self.reg[s[:-1]] & 0xffffffff
            elif s[-1] == "w":
                return self.reg[s[:-1]] & 0xffff
            elif s[-1] == "b":
                return self.reg[s[:-1]] & 0xff
            else:
                return self.reg[s]
        else:
            return self.reg[s]
    def Setreg(self, dst, value):
        if dst == "al" or dst == "bl" or dst == "cl" or dst == "dl":
            self.reg["r" + dst.replace("l","x")] = (value & 0xff)
        elif dst == "ah" or dst == "bh" or dst == "ch" or dst == "dh":
            self.reg["r" + dst.replace("h","x")] = ((value) << 8)
        elif dst == "ax" or dst == "bx" or dst == "cx" or dst == "dx" or dst == "bp" or dst == "si" or dst == "di" or dst == "sp":
             self.reg["r" + dst] = (value & 0xffff)
        elif dst == "eax" or dst == "ebx" or dst == "ecx" or dst == "edx" or dst == "edi" or dst == "esi" or dst == "ebp" or dst == "esp":
            self.reg[dst.replace("e","r")] = (value & 0xffffffff)
        elif ("r8" in dst) or ("r9" in dst) or ("r10" in dst) or ("r11" in dst) or ("r12" in dst) or ("r13" in dst) or ("r14" in dst) or (("r15" in dst)):
            if dst[-1] == "d":
                self.reg[dst[:-1]] = value & 0xffffffff
            elif dst[-1] == "w":
                self.reg[dst[:-1]] = value & 0xffff
            elif dst[-1] == "b":
                self.reg[dst[:-1]] = value & 0xff
            else:
                self.reg[dst] = value
        else:
            self.reg[dst] = value
    def Setregwithregname(self, dst, src):
        if dst == "al" or dst == "bl" or dst == "cl" or dst == "dl":
            self.reg["r" + dst.replace("l","x")] = (self.Getregwithname(src) & 0xff)
        elif dst == "ah" or dst == "bh" or dst == "ch" or dst == "dh":
            self.reg["r" + dst.replace("h","x")] = ((self.Getregwithname(src)) << 8)
        elif dst == "ax" or dst == "bx" or dst == "cx" or dst == "dx" or dst == "bp" or dst == "si" or dst == "di" or dst == "sp":
             self.reg["r" + dst] = (self.Getregwithname(src))
        elif dst == "eax" or dst == "ebx" or dst == "ecx" or dst == "edx" or dst == "edi" or dst == "esi" or dst == "ebp" or dst == "esp":
            self.reg[dst.replace("e","r")] = (self.Getregwithname(src))
        elif ("r8" in dst) or ("r9" in dst) or ("r10" in dst) or ("r11" in dst) or ("r12" in dst) or ("r13" in dst) or ("r14" in dst) or (("r15" in dst)):
            if dst[-1] == "d":
                self.reg[dst[:-1]] = self.Getregwithname(src) & 0xffffffff
            elif dst[-1] == "w":
                self.reg[dst[:-1]] = self.Getregwithname(src) & 0xffff
            elif dst[-1] == "b":
                self.reg[dst[:-1]] = self.Getregwithname(src) & 0xff
            else:
                self.reg[dst] = self.Getregwithname(src)
        else:
            self.reg[dst] = self.Getregwithname(src)
    def updaterip(self, key):
        if key != -1:
            self.reg["rip"] = key
    def updatestackreg(self, acquire_stack_flag):
        logstack("comparison:")
        logstack(self.reg["rsp"])
        logstack(self.reg["rspbegin"])
        self.reg["stack"] = self.reg["rspbegin"] - self.reg["rsp"]   ## catch the maximum stack, but I use min because stack is negative.
        logstack(self.reg["stack"])