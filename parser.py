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
        push_count = 0
        push_maxcount = 0
        stacklist = []
        headlist = []

        for data in contents:
            start = re.search("<*>:", data.strip())
            if start:
                stacklist.append(max(stacksize,push_maxcount << 2))
                headlist.append(data.strip())
                stacksize = 0
                push_count = 0
                push_maxcount = 0
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
                        instruction = temp
                        dst = instruction[1].split(",")[1]
                        src = instruction[1].split(",")[0]
                        searchexception1 = re.search("\(", dst)
                        searchexception2 = re.search("\(", src)
                        if ( not searchexception1 and not searchexception2):
                            if (re.match("\$0x*", src)):
                                size = int(src.replace("$",""), 16)
                                stacksize = max(stacksize, size)

                ## try to catch push instruction for rsp.
                searchpush = re.match("push", inst)
                searchpop = re.match("pop", inst)

                if searchpush:
                    push_count += 1
                    push_maxcount = max(push_count, push_maxcount)
                if searchpop:
                    push_count -= 1
        stacklist.append(stacksize)
        stacklist = stacklist[1:]  ## skip the first loop.
        ## print(headlist)
        ## print(stacklist)
        content = stacklist
        f.close()

    def parserecurrence(self):
        stacklist = []
        headlist = []
        with open(self.path) as f:
            contents = f.readlines()
        function_name = ""
        for data in contents:
            start = re.search("<*>:", data.strip())
            if start:
                function_name = data.strip().split("<")[1].replace(">:","")
            temp = data.strip().split("\t")
            for inst in temp:            
                searchcall = re.search("call", inst)
                if (searchcall):
                    searchrecurrsion = re.search(function_name, inst)
                    if (searchrecurrsion):
                        print("yes")
                
                
if __name__ == '__main__':
    parser = parser("a.s")
    parser.parseheader()
    parser.parsecontent()
    parser.parserecurrence()