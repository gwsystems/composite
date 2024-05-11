
DEBUG = False
DEBUGinst = False
DEBUGresult = True
def loginst(*argv):
    if DEBUGinst:
        print(argv)

def log(*argv):
    if DEBUG:
        print(argv)
        
def logresult(*argv):
    if DEBUGresult:
        print(argv)
