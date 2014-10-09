import sys, random, os, datetime

try:
	ncpus = sys.argv[1]
	cos_pid = sys.argv[2]
except:
        print "Warning: Missing params for SMP affinity setting. Check the python script."; sys.exit(1);
            
# Note: This currently ignores NUMA (cpuset.mems) setting: only NUMA node 0 is used.
def start():
	if int(ncpus) < 1:
	        print "Warning: SMP affinity not setting correctly. Check the number of CPUs in config." 
		sys.exit(1)
        os.system("echo -1 > /proc/sys/kernel/sched_rt_runtime_us")
        os.system("echo \"ENABLED=0\" > /etc/default/irqbalance")
        os.system("echo Setting CPU affinity... This could take several seconds for the first run after boot.")

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

	if int(ncpus) > 1:
		cos_cpu = int(ncpus) - 2
	else:
		cos_cpu = 0
	os.system("echo 0-" + str(cos_cpu) +  " > /dev/cpuset/cos/cpuset.cpus")
	os.system("echo 0 > /dev/cpuset/cos/cpuset.mems")
	os.system("echo " + cos_pid + " > /dev/cpuset/cos/tasks")
	# The following command takes quite a few seconds. It tries to
	# migrate all Linux tasks (except non-migratable tasks) to the
	# Linux core.
	os.system("for i in `cat /dev/cpuset/tasks`; do echo $i > /dev/cpuset/linux/tasks; done")

        if os.path.exists("/proc/irq/0/smp_affinity"):
                irq_mask = hex((1 << (linux_cpu)))[2:]
                l = len(irq_mask)
                if irq_mask[l-1] == 'L': # remove the L at the end
                        assert(l > 2)
                        irq_mask = irq_mask[:l-1]
                else:
                        irq_mask = irq_mask[:l]

                # then we need to add comma in the string!
                l = len(irq_mask)
                # determine how many commas we need
                if l % 8 == 0:
                        k = l / 8 - 1
                else:
                        k = l / 8

		mask = ""
                for i in range(0, k):
                        mask = irq_mask[l-8:l]
                        mask = ',' + mask
                        assert (l > 8)
                        l -= 8;
                mask = irq_mask[:l] + mask
                # finally we got the correct format
                os.system("echo " + mask + " > /proc/irq/default_smp_affinity;")
                os.system("for i in `ls /proc/irq/`; do if \\[ \"$i\" != \"default_smp_affinity\" \\]; then echo " + mask + " > /proc/irq/$i/smp_affinity; fi; done")

        os.system("echo Setting CPU affinity done.")

start()
