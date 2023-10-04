import re
class parser:
    def __init__(self, path):
        self.header = []
        self.content = []
        self.path = path
    def parseheader(self):
        with open(self.path) as f:
            contents = f.readlines()
        for data in contents:
            result = re.search("<*>:", data)
            if result:
                self.header.append(data)
        f.close()

    def parsecontent(self):
        with open(self.path) as f:
            contents = f.readlines()
        stacksize = 0
        stacklist = []
        headlist = []

        for data in contents:
            start = re.search("<*>:", data.strip())
            if start:
                stacklist.append(stacksize)
                headlist.append(data.strip())
                ## print(stacksize)
                stacksize = 0
                continue
            temp = data.strip().split("\t")
            for inst in temp:
                
                ## check move rbp/ebp as dest, 
                searchrbp = re.search("rbp", inst)
                searchebp = re.search("ebp", inst)
                searchminus = re.search("-", inst)
                searchmov = re.match("mov", inst)
                ## try to catch mov instruction.
                if (searchrbp or searchebp) and searchminus and searchmov: 
                    searchrbp = re.search("rbp", inst.split(",")[-1])  ## search the destination is rbp and ebp, dst in in the last index split string.
                    searchebp = re.search("ebp", inst.split(",")[-1])

                    if (searchrbp or searchebp):
                        val = inst.split(",")[-1].split("(")[0].replace("-", "")    ## get dst of inst. For example, mov    %r12b,-0x1(%rbp)   
                        if (re.match("0x*", val)):
                            size = int(val, 16)
                            stacksize = max(stacksize, size)
                        else:
                            val = inst.split(" ")[-1].split(",")[0].split("(")[0].replace("-", "")   ## get the source of inst. For example, movzbl -0xc(%rdx),%ebp
                            size = int(val, 16)
                            stacksize = max(stacksize, size)

                ## try to catch sub instruction for rsp.
                searchrbp = re.search("rbp", inst)
                searchebp = re.search("ebp", inst)
                searchrsp = re.search("rsp", inst)
                searchsub = re.match("sub", inst)
                if (searchrbp or searchebp or searchrsp) and searchsub: 
                    searchrbp = re.search("rbp", inst.split(",")[-1])  ## search the destination is rbp and ebp, dst in in the last index split string.
                    searchebp = re.search("ebp", inst.split(",")[-1])
                    searchrsp = re.search("rsp", inst.split(",")[-1])
                    if (searchrbp or searchebp or searchrsp):
                        temp = []
                        for i in inst.split(" "):  ## remove the blank
                            if len(i)!=0:
                                temp.append(i)

                        inst = temp
                        dst = inst[1].split(",")[1]
                        src = inst[1].split(",")[0]
                        searchexception1 = re.search("\(", dst)
                        searchexception2 = re.search("\(", src)
                        if ( not searchexception1 and not searchexception2):
                            if (re.match("\$0x*", src)):
                                size = int(src.replace("$",""), 16)
                                stacksize = max(stacksize, size)

        stacklist.append(stacksize)
        stacklist = stacklist[1:]  ## skip the first loop.
        print(headlist)
        print(stacklist)
        f.close()


if __name__ == '__main__':
    parser = parser("a2.s")
    parser.parseheader()
    parser.parsecontent()