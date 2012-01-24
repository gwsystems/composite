import sys, os, datetime, numpy

try:
    generator = sys.argv[1]
    runs_num = int(sys.argv[2])
except:
        print "Usage:", sys.argv[0], "run run_nums "; sys.exit(1)

do_stack = 1
do_cbuf = 0
do_stk_PIP = 0
do_cbuf_PIP = 0
do_switch = 0
do_inv = 0


# invocations related
def collect_inv_cost():

        result_temp_inv = "Result/tmp_micro_result_inv" 
        os.system("dmesg|grep \"invs\" > " + result_temp_inv)       
        with open(result_temp_inv,'r') as result_temp_inv_file:
            for line in result_temp_inv_file:
                value = int(line[line.rfind('invs') + 5:line.rfind("cycs") - 1])
                value = value/1000000
                os.system("echo " + str(value) + " >> " + result_inv)
        os.system("rm " + result_temp_inv)                               


# context switch related
def collect_switch_cost():

        result_temp_switch = "Result/tmp_micro_result_switch" 
        os.system("dmesg|grep \"switchs\" > " + result_temp_switch)       
        with open(result_temp_switch,'r') as result_temp_switch_file:
            for line in result_temp_switch_file:
                value = int(line[line.rfind('switchs') + 7:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_switch)
        os.system("rm " + result_temp_switch)                               


# stack related
def collect_stk_cost():

        result_temp_stk = "Result/tmp_micro_result_stk" 
        os.system("dmesg|grep \"took\" > " + result_temp_stk)       
        with open(result_temp_stk,'r') as result_temp_stk_file:
            for line in result_temp_stk_file:
                value = int(line[line.rfind('took') + 5:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_stk)
        os.system("rm " + result_temp_stk)                               

# stk_PIP related
def collect_stkPIP():
        global result_stkPIP
        result_temp_stkPIP = "Result/tmp_micro_result_stkPIP" 
        os.system("dmesg|grep \"stkPIP\" > " + result_temp_stkPIP)       
        with open(result_temp_stkPIP,'r') as result_temp_stkPIP_file:
            for line in result_temp_stkPIP_file:
                value = int(line[line.rfind('stkPIP') + 7:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_stkPIP)
        with open(result_stkPIP, 'a') as res_file:
                res_file.write('\n') 
        os.system("rm " + result_temp_stkPIP)                               

def process_stkPIP():
    global stkPIP_stat
    stkPIP_avg = 0
    stkPIP_stdev = 0
    tmp_array = []
    with open(result_stkPIP) as stkPIP_file:
        for line in stkPIP_file:
            if not line.strip():
                int_array = [int(s) for s in tmp_array]
                stkPIP_avg = round(numpy.average(int_array),0)
                stkPIP_stdev = round(numpy.std(int_array),0)
                os.system("echo " + str(stkPIP_avg) + " " + str(stkPIP_stdev) + " >> " + stkPIP_stat)
                tmp_array = []                
            else:
                tmp_array.append(line)


# cbuf_PIP related
def collect_cbufPIP():
        global result_cbufPIP
        result_temp_cbufPIP = "Result/tmp_micro_result_cbufPIP" 
        os.system("dmesg|grep \"cbufPIP\" > " + result_temp_cbufPIP)       
        with open(result_temp_cbufPIP,'r') as result_temp_cbufPIP_file:
            for line in result_temp_cbufPIP_file:
                value = int(line[line.rfind('cbufPIP') + 8:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_cbufPIP)
        with open(result_cbufPIP, 'a') as res_file:
                res_file.write('\n') 
        os.system("rm " + result_temp_cbufPIP)                               


def process_cbufPIP():
    global cbufPIP_stat
    cbufPIP_avg = 0
    cbufPIP_stdev = 0
    tmp_array = []
    with open(result_cbufPIP) as cbufPIP_file:
        for line in cbufPIP_file:
            if not line.strip():
                int_array = [int(s) for s in tmp_array]
                cbufPIP_avg = round(numpy.average(int_array),0)
                cbufPIP_stdev = round(numpy.std(int_array),0)
                os.system("echo " + str(cbufPIP_avg) + " " + str(cbufPIP_stdev) + " >> " + cbufPIP_stat)
                tmp_array = []
            else:
                tmp_array.append(line);


# cbuf related

def collect_alloc():
        global result_alloc
        result_temp_alloc = "Result/tmp_micro_result_alloc" 
        os.system("dmesg|grep \"alloc_cbuf\" > " + result_temp_alloc)       
        with open(result_temp_alloc,'r') as result_temp_alloc_file:
            for line in result_temp_alloc_file:
                value = int(line[line.rfind('cbuf') + 5:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_alloc)
        with open(result_alloc, 'a') as res_file:
                res_file.write('\n') 
        os.system("rm " + result_temp_alloc)                               

def collect_free():
        global result_free
        result_temp_free = "Result/tmp_micro_result_free" 
        os.system("dmesg|grep \"free_cbuf\" > " + result_temp_free)       
        with open(result_temp_free,'r') as result_temp_free_file:
            for line in result_temp_free_file:
                value = int(line[line.rfind('cbuf') + 5:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_free)
        with open(result_free, 'a') as res_file:
                res_file.write('\n')      
        os.system("rm " + result_temp_free)            


def collect_bf2bf():
        global result_bf2bf
        result_temp_bf2bf = "Result/tmp_micro_result_bf2bf" 
        stamp = str(datetime.datetime.now().strftime("%Y-%m-%d-%H:%M"))
        os.system("dmesg|grep \"b2bcost\" > " + result_temp_bf2bf)       
        with open(result_temp_bf2bf,'r') as result_temp_bf2bf_file:
            for line in result_temp_bf2bf_file:
                value = int(line[line.rfind('b2bcost') + 8:line.rfind("cycs") - 1])
                os.system("echo " + str(value) + " >> " + result_bf2bf)
        with open(result_bf2bf, 'a') as res_file:
                res_file.write('\n')
        os.system("rm " + result_temp_bf2bf)

def process_alloc():
    global alloc_stat
    alloc_avg = 0
    alloc_stdev = 0
    tmp_array = []
    with open(result_alloc) as alloc_file:
        for line in alloc_file:
            if not line.strip():
                int_array = [int(s) for s in tmp_array]
                alloc_avg = round(numpy.average(int_array),0)
                alloc_stdev = round(numpy.std(int_array),0)
                os.system("echo " + str(alloc_avg) + " " + str(alloc_stdev) + " >> " + alloc_stat)
            else:
                tmp_array.append(line);

def process_free():
    global free_stat
    free_avg = 0
    free_stdev = 0
    tmp_array = []
    with open(result_free) as free_file:
        for line in free_file:
            if not line.strip():
                int_array = [int(s) for s in tmp_array]
                free_avg = round(numpy.average(int_array),0)
                free_stdev = round(numpy.std(int_array),0)
                os.system("echo " + str(free_avg) + " " + str(free_stdev) + " >> " + free_stat)
            else:
                tmp_array.append(line);


def process_bf2bf():
    global bf2bf_stat
    bf2bf_avg = 0
    bf2bf_stdev = 0
    tmp_array = []
    with open(result_bf2bf) as bf2bf_file:
        for line in bf2bf_file:
            if not line.strip():
                int_array = [int(s) for s in tmp_array]
                bf2bf_avg = round(numpy.average(int_array),0)
                bf2bf_stdev = round(numpy.std(int_array),0)
                os.system("echo " + str(bf2bf_avg) + " " + str(bf2bf_stdev) + " >> " + bf2bf_stat)
            else:
                tmp_array.append(line);
                
def run_scripts():
    global result_alloc, result_free, result_bf2bf, free_stat, alloc_stat, bf2bf_stat, result_stk, result_stkPIP, result_cbufPIP, stkPIP_stat, cbufPIP_stat, result_switch, result_inv
    result_alloc = "Result/alloc_result"
    result_free = "Result/free_result"
    result_bf2bf = "Result/bf2bf_result"

    result_stk = "Result/stk_result_smn"
    result_switch = "Result/switch_result" 
    result_inv = "Result/invocation_result"
    
    alloc_stat = "Result/stat_alloc"
    bf2bf_stat = "Result/stat_bf2bf"
    free_stat = "Result/stat_free"

    result_stkPIP = "Result/stk_result_PIP"
    result_cbufPIP = "Result/cbuf_result_PIP"
    stkPIP_stat = "Result/stat_stkPIP"
    cbufPIP_stat = "Result/stat_cbufPIP"


    for i in range(runs_num):
        stamp = str(datetime.datetime.now().strftime("%Y-%m-%d-%H:%M"))
        os.system("echo ");
        os.system("echo " + "'<<<<< '" + str(runs_num-i) + "' >>>>>'" );
        if (do_inv or do_stack or do_cbuf):
            infilename = 'lubench1.sh'
        if (do_switch or do_stk_PIP or do_cbuf_PIP):
            infilename = 'lubench2.sh'
        os.system("dmesg -c >> dmesg.dump && sleep 1 && make && sleep 2 && sh " + infilename + ">> running.log && sleep 1")
        dmesg_saved = "Result/micro_dmesg_saved." + stamp
        os.system("dmesg >> " + dmesg_saved)

        if (do_inv == 1):
            collect_inv_cost()

        if (do_switch == 1):
            collect_switch_cost()

        if (do_stk_PIP == 1):
            collect_stkPIP()

        if (do_cbuf_PIP == 1):
            collect_cbufPIP()
       
        if (do_cbuf == 1):
            collect_alloc()
            collect_free()
            collect_bf2bf()
        if (do_stack == 1):
            collect_stk_cost()
    if (do_cbuf == 1):
        process_alloc()
        process_free()
        process_bf2bf()
    
    if (do_stk_PIP == 1):
        process_stkPIP()

    if (do_cbuf_PIP == 1):
        process_cbufPIP()

def start():
    os.system("echo -1 > /proc/sys/kernel/sched_rt_runtime_us")
    if (generator == 'run'):
        run_scripts()
    else:
        print "check the last input option!!"  
        
start()




        


