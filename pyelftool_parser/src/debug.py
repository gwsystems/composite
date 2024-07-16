DEBUG = False
DEBUGinst = True
DEBUGresult = False
DEBUGcall = False
DEBUGerror = True
def log(*argv):
    if DEBUG:
        print(argv)
def loginst(*argv):
    if DEBUGinst:
        print(argv)
def logresult(*argv):
    if DEBUGresult:
        print(argv)
def logcall(*argv):
    if DEBUGcall:
        print(argv)
def logerror(*argv):
    if DEBUGerror:
        print(argv)

