import sys, random, os, datetime

try:
    generator = sys.argv[1]
    mean_total_NRT_thds = int(sys.argv[2])
    mean_total_RT_thds = int(sys.argv[3])
    mean_period = int(sys.argv[4])
    mean_total_util = int(sys.argv[5])
    start_priority = int(sys.argv[6])
    start_num = int(sys.argv[7])
    end_num = int(sys.argv[8])
    thd_per_com = int(sys.argv[9])
except:
        print "Usage:", sys.argv[0], "\"gen\"/\"run\" NRT_thds RT_thds period utilization pri_0 start_num end_num thd_per_com"; sys.exit(1)

#default values
#mean_total_thds = 10
#mean_period = 10
#mean_total_util = 80
#start_priority = 7
#runs_num = 10
#thd_per_com = 1
#generator = gen   # gen : generate scripts   run:run

RT_thds_num = mean_total_RT_thds
run_RT_thds_num = RT_thds_num

if (thd_per_com > 0) :
    mean_total_util = mean_total_util/2
    run_RT_thds_num = RT_thds_num*(thd_per_com+1)

ff = 0
if ff == 1:
    run_mode = "default"
    call_mode = "default"
else:
    run_mode = "transient"
    call_mode = "default"
    event_mode = "true"

runs_num = end_num - start_num + 1
gen_num = 0

us_per_tick = 10000
       
staticFile = 'basic.sh'

thds_num = mean_total_RT_thds+mean_total_NRT_thds
period = [0] * thds_num
priority = [0] * thds_num
relativity = [0] * thds_num
utilization = [0] * thds_num   
execution_t = [0] * thds_num  

start_time= [0] * thds_num
duration_time = [0] * thds_num  

counter_i = 0

def init_para():

    global period, priority, utilization, execution_t, thds_num, RT_thds_num
    global gen_num

#initialize threads number        
    #thds_num = random.expovariate(1 / float(mean_total_RT_thds))
    #while(thds_num < mean_total_thds - 2 or thds_num > mean_total_thds + 2):
    #    thds_num = random.expovariate(1 / float(mean_total_RT_thds))
    thds_num = mean_total_RT_thds+mean_total_NRT_thds
    print 'threads num is', thds_num


# initialize period        
    min_period = 2
    flag = 0
    for i in range(RT_thds_num):
            while  (flag == 0):
                rand_p = int(random.expovariate(1 / float(mean_period)))
                if (rand_p >= min_period):
                    flag = 1
                    period[i] = rand_p
            flag = 0

#initialize utilization
    const_util = mean_total_util
    u_limit = const_util
    l_limit = const_util
    mu = float(mean_total_util) / RT_thds_num
    t_sum = 0
    min_util = 1
    flag = 0
    while(t_sum < l_limit or t_sum > u_limit):
        for i in range(RT_thds_num):
            while  (flag == 0):
                rand_u = int(random.expovariate(1 / mu))
                if (rand_u >= min_util):
                    flag = 1
                    utilization[i] = rand_u
            flag = 0                
        t_sum = sum(utilization)

    print 'total util is', t_sum
    
#initialize execution time
    temp_com = [0] * thds_num   
    for i in range(thds_num):
        execution_t[i] = utilization[i] * period[i] * us_per_tick / 100
        temp_com[i] = period[i]

    kk = 0
    j = 0
    for i in sorted(temp_com):
        if i == j:
            kk = kk - 1
        k = temp_com.index(i)
        priority[k] = start_priority + kk
        kk = kk + 1
        j = i
        temp_com[k] = -1

    if (run_mode == "transient"):
    # for transient threads
        #group1 = round(float(thds_num)/RT_thds_num)*1
        #group2 = round(float(thds_num)/RT_thds_num)*2
        #group2 = int(thds_num)

        group1 = RT_thds_num

    # initialize duration/start time   
        for i in range(thds_num):
            if (i < group1):
                duration_time[i] = 120
                start_time[i] = 0
            else:
                duration_time[i] = 20   # seconds             
                if (event_mode == "true"):
                    priority[i] = 30
                    period[i] = 0
                    utilization[i] = 0
                    execution_t[i] = 20000
#                    execution_t[i] = 100000
                    start_time[i] = 10
                else:
                    start_time[i] = 20
            
    genlog = 'Scripts/gen_log'
    gen_num = gen_num + 1  
    with open(genlog, 'a') as log_file:
        log_file.write('*****' + str(gen_num) + '*****\n')
        log_file.write('total util:' + str(t_sum) + '\n')
        log_file.write('period:' + str(period) + '\n')
        log_file.write('priority:' + str(priority) + '\n')
        log_file.write('utilization:' + str(utilization) + '\n')
        log_file.write('execution:' + str(execution_t) + '\n')
        if(run_mode == "transient"):
            log_file.write('start_time:' + str(start_time) + '\n')
            log_file.write('duration_time:' + str(duration_time) + '\n')
            
    print ('******'+str(gen_num)+'******')       
    print 'period', period
    print 'priority', priority
    print 'utilization', utilization
    print 'execution', execution_t
    if(run_mode == "transient"):
        print 'start time', start_time
        print 'duration time', duration_time

def set_util(ofile):
    global counter_i
    kk = 0
    temp = priority[0]
    for i in range(thds_num):
        P = priority[i]
        T = period[i]
        C = execution_t[i]
        S = start_time[i]
        D = duration_time[i]

        if(0):#kk < thd_per_com):
            if(kk==0):
                ofile.write('(!p' + str(counter_i) + '.o=exe_pt.o),a' + str(temp) +'\'')
                counter_i = counter_i + 1
            relativity[i] = priority[i]-temp
            R = relativity[i]
            ofile.write('p' + str(T) + ' e' + str(C) + ' s' + str(S)+ ' d' + str(D)+' r'+ str(R) )            
            kk = kk+1
            if(kk == thd_per_com) :
                kk = 0
                if( i < thds_num-1 ):
                    temp = priority[i+1]
                ofile.write('\''+';\\\n')                    
            else:
                if(i < thds_num-1):                
                    ofile.write( ' ')

        if(run_mode == "transient"):
            ofile.write('(!p' + str(i) + '.o=exe_pt.o),a' + str(P) + '\'' + 'p' + str(T) + ' e' + str(C) + ' s' + str(S)+ ' d' + str(D) + ' m' + str(thd_per_com))
            ofile.write('\''+';\\\n')
        else:
            ofile.write('(!p' + str(i) + '.o=exe_pt.o),a' + str(P) + '\'' + 'p' + str(T) + ' e' + str(C) + '\'')
            ofile.write(';\\\n')
        
def set_thds(ofile):
    global counter_i
    #for i in range(counter_i):
    for i in range(thds_num):
#        temp = random.randint(0, 1)
#        if temp < 0.5:  # 9-14
        if (period[i] > 0) :
            if (call_mode == "uniform_call"):
                call_left_id = str(int(round(random.uniform(9,14))))
                ofile.write('p' + str(i) + '.o-te.o|fprr.o|schedconf.o|print.o|sh'+call_left_id+'.o|sm.o')
            else:
                ofile.write('p' + str(i) + '.o-te.o|fprr.o|schedconf.o|print.o|sh18.o|sm.o')
        else:           # 15-20
            if (call_mode == "uniform_call"):
               call_right_id = str(int(round(random.uniform(15,20))))
               ofile.write('p' + str(i) + '.o-te.o|fprr.o|schedconf.o|print.o|sh'+call_right_id+'.o|sm.o')                
            else:
                ofile.write('p' + str(i) + '.o-te.o|fprr.o|schedconf.o|print.o|sh12.o|sm.o')            
        ofile.write(';\\\n')
        
def gen_scripts():
    global runs_num
    os.system("cd Scripts && rm * && cd ..")
    for i in range(runs_num):
        init_para()
        outfilename = 'Scripts/ltest' + str(i + 1) + '.sh'
        with open(outfilename, 'w') as ofile:
            with open(staticFile, 'r') as ifile:
                for line in ifile:
                    if line == '# define utilization\n':
                        set_util(ofile)
                    elif line == '# define threads\n':
                        set_thds(ofile)
                    else:
                        ofile.write(line)

def run_scripts():
    global runs_num, RT_thds_num
    stamp = str(datetime.datetime.now().strftime("%Y-%m-%d-%H:%M"))
    result = "Result/result." + stamp
    # os.system("echo \#\#" + "-" * 10 + " >> " + result)
    for i in range(runs_num):
        x_axis = 0
        infilename = 'Scripts/ltest' + str(i + start_num) + '.sh'            
        os.system("dmesg -c >> dmesg.dump && sleep 1 && make && sleep 2 && sh " + infilename + ">> running.log && sleep 1")
        result_temp = "Result/result_temp"
        result_temp_ava = "Result/result_temp_ava"
        dmesg_saved = "Result/dmesg_saved." + stamp
        os.system("dmesg >> " + dmesg_saved)
        os.system("dmesg|grep \"allocated\" > " + result_temp_ava)
        os.system("dmesg|grep \"Thread\" > " + result_temp)
        with open(result_temp_ava,'r') as result_temp_ava_file:
            with open(result_temp, 'r') as result_temp_file:
                sum2 = sum = max = index = 0
                for line in result_temp_file:
                    value = int(line[line.rfind(',') + 2:line.rfind("miss") - 1])
                    dlm = int(line[line.rfind("M") + 1:line.rfind(',')])
                    sum2 = sum2 + dlm
                    sum = sum + value
                    if max < value :
                        max = value
                    index = index + 1
                    if (((ff == 0) and (event_mode == "true") and index == run_RT_thds_num+1) or index == thds_num + 1):
                        x_axis = x_axis + 1
                        ava_line = result_temp_ava_file.readline()
                        ava_val = int(ava_line[ava_line.find(':') + 2:ava_line.find(',')])
                        os.system("echo " + str(x_axis) + " " + str(max) + " " + str(sum) + " " + str(ava_val) + " " + str(sum2) + " >> " + result)
                        os.system("echo " + str(x_axis) + " " + str(max) + " " + str(sum) + " " + str(ava_val) + " " + str(sum2))
                        sum2 = sum = max = index = 0
                os.system("echo \#\#" + "-" * 10 + infilename+ " finished!! " +" >> " + result)
                os.system("echo " + infilename)
            with open(result, 'a') as res_file:
                res_file.write('\n')
                res_file.write('\n')

                                 
def start():
    os.system("echo -1 > /proc/sys/kernel/sched_rt_runtime_us")
    if (generator == 'gen'):
        gen_scripts()
    elif (generator == 'run'):
        run_scripts()
    else:
        print "check the last input option!!"    
  
start()




        


