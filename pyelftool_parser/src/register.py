
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
        self.reg["rsp"] = 0
        self.reg["r8"] = 0
        self.reg["r9"] = 0
        self.reg["r10"] = 0
        self.reg["r11"] = 0
        self.reg["r12"] = 0
        self.reg["r13"] = 0
        self.reg["r14"] = 0
        self.reg["r15"] = 0
        self.reg["eax"] = 0
        self.reg["ebx"] = 0
        self.reg["ecx"] = 0
        self.reg["edx"] = 0
        self.reg["edi"] = 0 
        self.reg["esi"] = 0
        self.reg["ebp"] = 0
        self.reg["esp"] = 0
        self.reg["xmm0"] = 0
        self.reg["xmm1"] = 0
        self.reg["xmm2"] = 0
        self.reg["xmm3"] = 0
        self.reg["xmm4"] = 0
        self.reg["xmm5"] = 0
        self.reg["xmm6"] = 0
        self.reg["xmm7"] = 0
        self.reg["stack"] = 0
    def clean(self):
        self.reg["rax"] = 0
        self.reg["rbx"] = 0
        self.reg["rcx"] = 0
        self.reg["rdx"] = 0
        self.reg["rdi"] = 0 
        self.reg["rsi"] = 0
        self.reg["rbp"] = 0
        self.reg["rsp"] = 0
        self.reg["r8"] = 0
        self.reg["r9"] = 0
        self.reg["r10"] = 0
        self.reg["r11"] = 0
        self.reg["r12"] = 0
        self.reg["r13"] = 0
        self.reg["r14"] = 0
        self.reg["r15"] = 0
        self.reg["eax"] = 0
        self.reg["ebx"] = 0
        self.reg["ecx"] = 0
        self.reg["edx"] = 0
        self.reg["edi"] = 0 
        self.reg["esi"] = 0
        self.reg["ebp"] = 0
        self.reg["esp"] = 0
        self.reg["xmm0"] = 0
        self.reg["xmm1"] = 0
        self.reg["xmm2"] = 0
        self.reg["xmm3"] = 0
        self.reg["xmm4"] = 0
        self.reg["xmm5"] = 0
        self.reg["xmm6"] = 0
        self.reg["xmm7"] = 0
        self.reg["stack"] = 0
    def Isreg(self, s):
        if s in self.reglist:
            return True
        else:
            return False
    def updaterip(self, key):
        if key != -1:
            self.reg["rip"] = key
    def updatestackreg(self):
        self.reg["stack"] = min(self.reg["stack"], self.reg["rsp"])  ## catch the maximum stack, but I use min because stack is negative.