#!/usr/bin/python

import re
import sys
import math
import string

if (len(sys.argv) < 4):
    print "Usage: ./gen_cdf.py <file> <col> <incriment>"
    sys.exit(1)
    
f = open(sys.argv[1], 'r')

col = string.atoi(sys.argv[2])

incr = string.atof(sys.argv[3])

lines = f.readlines()

times = []

for line in lines:
    line = string.rstrip(line)
    data = re.split("\t", line)
    times.append((float)(data[col-1]))

# find the average
sum = 0
min = 2000000
max = -1
t = 0.0

times.sort();
#print times

first = 1
for time in times:
    if first == 1:
        min = time
        first = 0
        
    max = time


#print len(times)
#print str(min)+"\t"+ str(max)

count = 0
#step = 0.2
step = incr
how_many = len(times)

for val in range(0, ((max)*(1/step))+1):
    curr = min+float(val*step)

    while count < len(times) and times[count] <= curr:
        count = count + 1

    percent = float(count)/float(how_many)
    
    print str(curr)+"\t"+str(percent)

#    if (percent >= 0.99):
#        break

f.close()
    

   
