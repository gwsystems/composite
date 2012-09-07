import sys, random, os, datetime

try:
	ncpus = sys.argv[1]
	cos_pid = sys.argv[2]
except:
        print "Warning: Missing params for SMP affinity setting. Check the python script."; sys.exit(1);
                                 
def start():
	if ncpus < 1:
	        print "Warning: SMP affinity not setting correctly. Check the number of CPUs in config." 
		sys.exit(1)

        os.system("echo -1 > /proc/sys/kernel/sched_rt_runtime_us")

	if not(os.path.isdir("/dev/cpuset")):
		os.system("mkdir -p /dev/cpuset")
	if not(os.path.isfile("/dev/cpuset/cpuset.cpus")):
		os.system("mount -t cgroup -ocpuset cpuset /dev/cpuset")

	if not(os.path.isdir("/dev/cpuset/linux")):
		os.system("mkdir -p /dev/cpuset/linux")
	if not(os.path.isdir("/dev/cpuset/cos")):
		os.system("mkdir -p /dev/cpuset/cos")

	linux_cpu = int(ncpus) - 1
	os.system("echo " + str(linux_cpu) + " > /dev/cpuset/linux/cpuset.cpus")
	os.system("echo 0 > /dev/cpuset/linux/cpuset.mems")

	if ncpus > 1:
		cos_cpu = int(ncpus) - 2
	else:
		cos_cpu = 0
	os.system("echo 0-" + str(cos_cpu) +  " > /dev/cpuset/cos/cpuset.cpus")
	os.system("echo 0 > /dev/cpuset/cos/cpuset.mems")

	os.system("echo " + cos_pid + " > /dev/cpuset/cos/tasks")
	os.system("for i in `cat /dev/cpuset/tasks`; do echo $i > /dev/cpuset/linux/tasks; done")

	irq_mask = str(1 << (linux_cpu))
	os.system("for i in `ls /proc/irq/`; do if \\[ \"$i\" != \"default_smp_affinity\" \\]; then echo " + irq_mask + " > /proc/irq/$i/smp_affinity; fi; done")

start()
