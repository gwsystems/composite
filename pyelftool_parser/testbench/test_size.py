import unittest   # The test framework
import sys
sys.path.insert(0, '/home/spadek67424/work/research/pyelftool_parser/src')
from analyzer import *
from execute import *
from register import *

class Test_TestFunctionSize(unittest.TestCase):
    def test_size(self):
        path = "./testbench/selftest/a.elf"
        disassemble = disassembler(path)
        disassemble.disasmsymbol()
        disassemble.disasminst()
        regist = register()
        exe = execute(regist)
        parse = parser(disassemble.symbol, disassemble.inst, regist, exe)
        stacklist = driver(disassemble, regist, exe, parse)
        self.assertEqual(stacklist[0], -8)

    # This test is designed to fail for demonstration purposes.
    def test_decrement(self):
        self.assertEqual(4, 4)

if __name__ == '__main__':
    path = "/home/spadek67424/work/research/pyelftool_parser/testbench/selftest/a.elf"
    disassembler = disassembler(path)
    disassembler.disasmsymbol()
    disassembler.disasminst()
    register = register.register()
    execute = execute.execute(register)
    parser = parser(disassembler.symbol, disassembler.inst, register, execute)
    stackfunction, stacklist = parser.stack_analyzer()
    unittest.main()
    
    