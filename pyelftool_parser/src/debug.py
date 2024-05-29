DEBUG = False
DEBUGinst = False
DEBUGresult = False
DEBUGcall = False
DEBUGerror = False
def loginst(*argv):
    if DEBUGinst:
        print(argv)

def log(*argv):
    if DEBUG:
        print(argv)
        
def logresult(*argv):
    if DEBUGresult:
        print(argv)
def logcall(*argv):
    if DEBUGcall:
        print(argv)
def logcall(*argv):
    if DEBUGerror:
        print(argv)

