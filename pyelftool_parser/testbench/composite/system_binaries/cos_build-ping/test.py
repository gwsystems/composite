import unittest   # The test framework
import sys
sys.path.insert(0, '/home/minghwu/work/StackAnalyzer/pyelftool_parser/src')
import analyzer

class Test_TestFunctionSize(unittest.TestCase):
    def setUp(self):
        path = "./tests.unit_pingpong.global.ping"
        self.disassemble = analyzer.disassembler(path)
        self.disassemble.disasmsymbol()
        self.disassemble.disasminst()
        self.reg = analyzer.register.register()
        self.reg.reg["pc"] = self.disassemble.entry_pc
        self.exe = analyzer.execute.execute(self.reg)
        self.parser = analyzer.parser(self.disassemble.symbol, self.disassemble.inst, 
                    self.reg, self.exe, 
                    self.disassemble.exit_pc, self.disassemble.acquire_stack_address)
        self.stacklist = analyzer.driver(self.disassemble, self.parser)
    def test(self):
        ## test printf_core
        self.assertEqual(self.stacklist[18], -496)
        ## test printc
        self.assertEqual(self.stacklist[12], -416)
if __name__ == '__main__':
    path = "./tests.unit_pingpong.global.ping"
    disassembler = analyzer.disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    register = analyzer.register.register()
    execute = analyzer.execute.execute(register)
    parser = analyzer.parser(disassembler.symbol, disassembler.inst, 
                    register, execute, 
                    disassembler.exit_pc, disassembler.acquire_stack_address)
    unittest.main()
    
    