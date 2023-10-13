
class register:
    def __init__(self):
        self.reglist = ["rax", "rbx", "rcx", "rdx","rdi" , "rsi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]
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
    def clear(self, reg):
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
    def Isreg(self, s):
        if s in self.reglist:
            return True
        else:
            return False
    def updaterip(self, key):
        if key != -1:
            self.reg["rip"] = key

