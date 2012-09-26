#include <cos_component.h>
#include <print.h>
#include <sched.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define SEED 35791246

void cos_init(void *args)
{
    int niter=0;
    double x,y;
    int i,count=0; /* # of points in the 1st quadrant of unit circle */
    double z;
    double pi;

    /* initialize random numbers */
    srand(SEED);
    count=0;
    for ( i=0; i<1024; i++) {
       x = (double)rand()/RAND_MAX;
       y = (double)rand()/RAND_MAX;
       z = x*x+y*y;
       if (z<=1) count++;
    }
    pi=(double)count/niter*4;
    printc("# of trials= %d , estimate of pi is %g \n",niter,pi);
    return 0;
}

