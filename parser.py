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
            for text in temp:
                
                ## check move rbp/ebp as dest, 
                searchrbp = re.search("rbp", text)
                searchebp = re.search("ebp", text)
                searchminus = re.search("-", text)
                searchmov = re.match("mov", text)
                ## try to catch mov instruction.
                if (searchrbp or searchebp) and searchminus and searchmov: 
                    searchrbp = re.search("rbp", text.split(",")[-1])  ## search the destination is rbp and ebp, dst in in the last index split string.
                    searchebp = re.search("ebp", text.split(",")[-1])

                    if (searchrbp or searchebp):
                        print(text)
                        val = text.split(",")[-1].split("(")[0].replace("-", "")    ## get dst of inst. For example, mov    %r12b,-0x1(%rbp)   
                        if (re.match("0x*", val)):
                            size = int(val, 16)
                            stacksize = max(stacksize, size)
                        else:
                            val = text.split(" ")[-1].split(",")[0].split("(")[0].replace("-", "")   ## get the source of inst. For example, movzbl -0xc(%rdx),%ebp
                            size = int(val, 16)
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