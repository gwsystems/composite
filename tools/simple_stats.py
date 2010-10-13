#!/usr/bin/python

import re
import sys
import math
import string

if (len(sys.argv) < 3):
    print "Usage: ./simple_stats.py <file> <col> <output_type>"
    sys.exit(1)
    
f = open(sys.argv[1], 'r')

col = string.atoi(sys.argv[2])
comp_output = sys.argv[3]

lines = f.readlines()

times = []

for line in lines:
    line = string.rstrip(line)
    data = re.split("\t", line)
    times.append(data[col-1])

#print times

# find the average
sum = 0
min = 2000000
max = -1
t = 0.0

for time in times:
    t = string.atof(time)
    if t > max:
        max = t
    if t < min:
        min = t
    sum = sum + t

len_times = (float)(len(times))

if len_times == 0:
    average = 0
else:
    average = (str)((float)(sum)/(float)(len_times))

# find the standard deviation
stddev = 0
for time in times:
    if len_times == 0:
        stddev = 0
    else:
        stddev = stddev + math.pow((string.atof(time)-(sum/len_times)), 2)
        
if len_times == 1:
    stddev = 0
else:
    stddev = stddev/(len_times-1)

stddev = math.sqrt(stddev)

if comp_output == "1":
    print str(sys.argv[1]) + "\t" + str(average) + "\t" + (str)(stddev) + "\t" + (str)(max) + "\t" + (str)(min)
else:
    print "Average: "+ str(average)
    print "Standard deviation: "+ (str)(stddev)
    print "Max: "+(str)(max)
    print "Min: "+(str)(min)

f.close()
    

   
