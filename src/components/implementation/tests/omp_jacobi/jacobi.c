#include <omp.h>
#include <math.h>
#include <stdio.h>

#define N 1024
#define M N

#define MEAS_ITER 100

#define NCPU 5
#define COS

#define DISABLE

#ifdef COS
#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_synchronization.h>
#include <mem_mgr_large.h>

#define printf printc
void *_GLOBAL_OFFSET_TABLE_ = NULL;

#define COS_MEM (92*1024*1024)

void *mem_addr = (void *)0x80000000;
void *hp = (void *)0x80000000;

#else

// Linux 
#include <stdio.h>
#include <stdlib.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

#define NUM_CPU 40
void set_smp_affinity()
{
	char cmd[64];
	int ret;
	/* everything done is the python script. */
	sprintf(cmd, "python set_smp_affinity.py %d %d", NUM_CPU, getpid());
	ret = system(cmd);
//	printf("ret from set_smp_affinity %d\n", ret);
}

//doesn't work
void set_openmp_affinity()
{
	int ret;
	printf("before getenv %s\n", getenv("GOMP_CPU_AFFINITY"));
	ret = setenv("GOMP_CPU_AFFINITY", "0-38", 1);
	printf("ret from set env %d, getenv %s\n", ret, getenv("GOMP_CPU_AFFINITY"));
	
	assert(ret == 0);
}

static void 
call_getrlimit(int id, char *name)
{
	struct rlimit rl;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}		
}

static void 
call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		perror("getrlimit: "); 
		exit(-1);
	}		
}

void
set_prio(void)//int prio, int cpu)
{
	struct sched_param sp;
	cpu_set_t set;

	call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
	call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");	
	call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);//prio
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: "); 
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
	}
//	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	/* CPU_ZERO(&set); */
	/* CPU_SET(cpu, &set); */
	/* if (sched_setaffinity(0, sizeof(set), &set)) { */
	/* 	perror("setaffinity: "); */
	/* 	exit(-1); */
	/* } */

	return;
}

#endif

void *OSCR_calloc(size_t nmemb, size_t size) {
  void *pointer;
  size_t tot = nmemb * size;

  /* pointer = calloc(nmemb, size); */
  /* if (pointer == NULL)  */
	  /* OSCR_error("Not enough memory!! No. of elements requested: ", nmemb); */
#ifdef COS
  pointer = hp;
  if (tot % CACHE_LINE) tot += (CACHE_LINE - tot % CACHE_LINE);
  assert(tot % CACHE_LINE == 0);
  hp += tot;
  printc("cos mem allocated %d MB in total.\n", (int)((hp - mem_addr)/1024/1024));
  if (unlikely(hp > mem_addr + COS_MEM)) {
	  printf("running out of memory!\n");
	  BUG();
  }

  /* if (unlikely(pointer == NULL)) { */
  /* 	  printf("running out of memory!\n"); */
  /* 	  BUG(); */
  /* } */
#else
  pointer = malloc(tot);
#endif

  memset(pointer, 0, tot);

  return (pointer);
}

void *OSCR_malloc(size_t size) {
  void *pointer;

#ifdef COS
  pointer = hp;
  if (size % CACHE_LINE) size += (CACHE_LINE - size % CACHE_LINE);
  assert(size % CACHE_LINE == 0);

  hp += size;
  printc("cos mem allocated %d MB in total.\n", (int)((hp - mem_addr)/1024/1024));

  if (unlikely(hp > mem_addr + COS_MEM)) {
	  printf("running out of memory!\n");
	  BUG();
  }

  /* if (unlikely(pointer == NULL)) { */
  /* 	  printf("running out of memory!\n"); */
  /* 	  BUG(); */
  /* } */
#else
  pointer = malloc(size);

  if (pointer == NULL)
	 printf("Not enough memory!! No. of bytes requested: ", size);
#endif

  return (pointer);
}

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

/* ***********************************************************************
  This program is part of the
        OpenMP Source Code Repository

        http://www.pcg.ull.es/ompscr/
        e-mail: ompscr@etsii.ull.es

   Copyright (c) 2004, OmpSCR Group
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, 
   are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright notice, 
       this list of conditions and the following disclaimer. 
     * Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution. 
     * Neither the name of the University of La Laguna nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission. 

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
   OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
   OF SUCH DAMAGE.

  FILE:              c_jacobi01.c
  VERSION:           1.1
  DATE:              Oct 2004
  AUTHORS:           Author:       Joseph Robicheaux, Kuck and Associates, Inc. (KAI), 1998
                     Modified:     Sanjiv Shah,       Kuck and Associates, Inc. (KAI), 1998
                     This version: Dieter an Mey,     Aachen University (RWTH), 1999 - 2003
                                   anmey@rz.rwth-aachen.de
                                   http://www.rwth-aachen.de/People/D.an.Mey.html
  COMMENTS TO:       ompscr@etsii.ull.es
  DESCRIPTION:       program to solve a finite difference discretization of Helmholtz equation : 
                     (d2/dx2)u + (d2/dy2)u - alpha u = f using Jacobi iterative method.
  COMMENTS:          OpenMP version 1: two parallel regions with one parallel loop each, the naive approach.  
                     Directives are used in this code to achieve paralleism. 
                     All do loops are parallized with default 'static' scheduling.
  REFERENCES:        http://www.rz.rwth-aachen.de/computing/hpc/prog/par/openmp/jacobi.html
  BASIC PRAGMAS:     parallel for
  USAGE:             ./c_jacobi01.par 5000 5000 0.8 1.0 1000
  INPUT:             n - grid dimension in x direction
                     m - grid dimension in y direction
                     alpha - Helmholtz constant (always greater than 0.0)
                     tol   - error tolerance for iterative solver
                     relax - Successice over relaxation parameter
                     mits  - Maximum iterations for iterative solver
  OUTPUT:            Residual and error 
                     u(n,m) - Dependent variable (solutions)
                     f(n,m) - Right hand side function 
  FILE FORMATS:      -
  RESTRICTIONS:      -
  REVISION HISTORY:
**************************************************************************/

//#include "OmpSCR.h"

#define U(i,j) u[(i)*n+(j)]
#define F(i,j) f[(i)*n+(j)]
#define NUM_ARGS  6
#define NUM_TIMERS 1

int n, m, mits;
double tol, relax, alpha;

void jacobi (int n, int m, double dx, double dy, 
             double alpha, double omega, 
             double *u, double *f, 
             double tol, int maxit, double* uold);

/******************************************************
* Initializes data 
* Assumes exact solution is u(x,y) = (1-x^2)*(1-y^2)
*
******************************************************/
void initialize(  
                int n,    
                int m,
                double alpha,
                double *dx,
                double *dy,
                double *u,
                double *f)
{
  int i,j,xx,yy;

  *dx = 2.0 / (n-1);
  *dy = 2.0 / (m-1);

  /* Initilize initial condition and RHS */
  for (j=0; j<m; j++){
    for (i=0; i<n; i++){
      xx = -1.0 + *dx * (i-1);
      yy = -1.0 + *dy * (j-1);
      U(j,i) = 0.0;
      F(j,i) = -alpha * (1.0 - xx*xx) * (1.0 - yy*yy)
                - 2.0 * (1.0 - xx*xx) - 2.0 * (1.0 - yy*yy);
    }
  }
      
}


/************************************************************
* Checks error between numerical and exact solution 
*
************************************************************/
void error_check(
                 int n,
                 int m,
                 double alpha,
                 double dx,
                 double dy,
                 double *u,
                 double *f)
{
  int i,j;
  double xx, yy, temp, error;

  dx = 2.0 / (n-1);
  dy = 2.0 / (n-2);
  error = 0.0;

  for (j=0; j<m; j++){
    for (i=0; i<n; i++){
      xx = -1.0 + dx * (i-1);
      yy = -1.0 + dy * (j-1);
      temp = U(j,i) - (1.0 - xx*xx) * (1.0 - yy*yy);
      error += temp*temp;
    }
  }

 error = sqrt(error)/(n*m);

  printf("Solution Error : %g\n", error);

}


unsigned long long cost[MEAS_ITER];

int main(int argc, char **argv){
	double *u, *f, dx, dy;
	double dt, mflops;
	int NUMTHREADS;
	char *PARAM_NAMES[NUM_ARGS] = {"Grid dimension: X dir =", "Grid dimension: Y dir =", "Helmhotlz constant =", 
				       "Successive over-relaxation parameter =", 
				       "error tolerance for iterative solver =", "Maximum iterations for solver ="}; 
	char *TIMERS_NAMES[NUM_TIMERS] = {"Total_time"};
	char *DEFAULT_VALUES[NUM_ARGS] = {"5000", "5000", "0.8", "1.0", "1e-7", "1000"};

#ifdef DISABLE
	return 0;
#endif

#ifndef COS
	printf("Setting highest prio...\n");
	set_prio();
	set_smp_affinity();
	//set_openmp_affinity();
	
	printf("Done.\n");
	printf("getenv %s, wait policy %s\n", getenv("GOMP_CPU_AFFINITY"), getenv("OMP_WAIT_POLICY"));
	omp_set_num_threads(NCPU);
#else
	//memory hack
	int ret;
	if ((ret = cos_vas_cntl(COS_VAS_SPD_EXPAND, cos_spd_id(), (long)mem_addr, COS_MEM))) {
		printc("ERROR: vas cntl returned %d\n", ret);
	}
	int ii;
	for (ii = 0; ii < COS_MEM / PAGE_SIZE; ii++) {
		vaddr_t addr;
		//printc("ii %d, mapping to %p\n", ii, mem_addr+ii*PAGE_SIZE);
		addr = mman_get_page(cos_spd_id(), (vaddr_t)mem_addr + ii * PAGE_SIZE, 0);
		if (addr != (vaddr_t)mem_addr + ii * PAGE_SIZE) {
			printc("mapping failed\n");
			return 0;
		}
	}
	
	{//test the mem we got
		int *addr = (int *)mem_addr;
		int k;
		for (k = 0; k < (int)(COS_MEM/sizeof(int)); k++) {
			addr[k] = k;
		}
		for (k = 0; k < (int)(COS_MEM/sizeof(int)); k++) {
			if (addr[k] != k)
				printc("k %d, expect k, actually %d\n", k, addr[k]);
		}
		
	}
	printc("memory allocation & test done\n");
#endif

	NUMTHREADS = omp_get_max_threads();
	/* OSCR_init (NUMTHREADS, "Jacobi Solver v1", "Use 'jacobi01' <n> <m> <alpha> <relax> <tol> <mits>", NUM_ARGS, */
	/*              PARAM_NAMES, DEFAULT_VALUES , NUM_TIMERS, NUM_TIMERS, TIMERS_NAMES, */
	/*              argc, argv); */

	n = N;//OSCR_getarg_int(1);
	m = M;//OSCR_getarg_int(2);
	alpha = 0.8;//OSCR_getarg_double(3);
	relax = 1.0;//OSCR_getarg_double(4);
	tol = 0.0000001;//OSCR_getarg_double(5);
	mits = 1000;//OSCR_getarg_int(6);

	printf("ncpu %d -> %d, %d, %g, %g, %g, %d\n",
	       NUMTHREADS, n, m, alpha, relax, tol, mits);
    
	u = (double *) OSCR_malloc(n*m*sizeof(double));
	f = (double *) OSCR_malloc(n*m*sizeof(double));

	double *uold;
	/* wegen Array-Kompatibilitaet, werden die Zeilen und Spalten (im Kopf)
	   getauscht, zB uold[spalten_num][zeilen_num]; bzw. wir tuen so, als ob wir das
	   gespiegelte Problem loesen wollen */
	uold = (double *)OSCR_malloc(sizeof(double) * n *m);

	/* arrays are allocated and initialzed */
	initialize(n, m, alpha, &dx, &dy, u, f);
    

	/* Solve Helmholtz eqiation */
//	OSCR_timer_start(0);
	int i;
	unsigned long long s, e;
	for (i = 0; i < MEAS_ITER; i++) {
		memset(u, 0, n*m*sizeof(double));
		memset(f, 0, n*m*sizeof(double));
		memset(uold, 0, n*m*sizeof(double));

		rdtscll(s);
		jacobi(n, m, dx, dy, alpha, relax, u,f, tol, mits, uold);
		rdtscll(e);
		cost[i] = e - s;
	}

//	OSCR_timer_stop(0);
//	dt = OSCR_timer_read(0);

//	printf(" elapsed time : %12.6f\n", dt);
//	mflops = (0.000001*mits*(m-2)*(n-2)*13) / dt;
//	printf(" MFlops       : %12.6g (%d, %d, %d, %g)\n",mflops, mits, m, n, dt);

	for (i = 0; i < MEAS_ITER; i++) {
		printf("cost %d: %llu\n", i, cost[i]);
	}

	error_check(n, m, alpha, dx, dy, u, f);
   
//	OSCR_report(1, TIMERS_NAMES); 
    
	return 0;
}



/* 
      subroutine jacobi (n,m,dx,dy,alpha,omega,u,f,tol,maxit)
******************************************************************
* Subroutine HelmholtzJ
* Solves poisson equation on rectangular grid assuming : 
* (1) Uniform discretization in each direction, and 
* (2) Dirichlect boundary conditions 
* 
* Jacobi method is used in this routine 
*
* Input : n,m   Number of grid points in the X/Y directions 
*         dx,dy Grid spacing in the X/Y directions 
*         alpha Helmholtz eqn. coefficient 
*         omega Relaxation factor 
*         f(n,m) Right hand side function 
*         u(n,m) Dependent variable/Solution
*         tol    Tolerance for iterative solver 
*         maxit  Maximum number of iterations 
*
* Output : u(n,m) - Solution 
*****************************************************************
*/
void jacobi (const int n, const int m, double dx, double dy, double alpha, 
	     double omega, double *u, double *f, double tol, int maxit, double *uold)
{
  int i,j,k;
  double error, resid, ax, ay, b;

  ax = 1.0/(dx * dx); /* X-direction coef */
  ay = 1.0/(dy*dy); /* Y_direction coef */
  b = -2.0/(dx*dx)-2.0/(dy*dy) - alpha; /* Central coeff */

  error = 10.0 * tol;

  k = 1;
  while (k <= maxit && error > tol) {

	error = 0.0;

	/* copy new solution into old */
#if NCPU > 1
#pragma omp parallel for private(i)
#endif
	for (j=0; j<m; j++)
		for (i=0; i<n; i++)
			uold[i + m*j] = u[i + m*j];


	/* compute stencil, residual and update */
#if NCPU > 1
#pragma omp parallel for reduction(+:error) private(i,resid)
#endif
	for (j=1; j<m-1; j++)
		for (i=1; i<n-1; i++){
			resid =(
				ax * (uold[i-1 + m*j] + uold[i+1 + m*j])
				+ ay * (uold[i + m*(j-1)] + uold[i + m*(j+1)])
				+ b * uold[i + m*j] - f[i + m*j]
				) / b;
		
			/* update solution */
			u[i + m*j] = uold[i + m*j] - omega * resid;

			/* accumulate residual error */
			error =error + resid*resid;

		}

	/* error check */
	k++;
    error = sqrt(error) /(n*m);

  } /* while */

  /* printf("Total Number of Iterations %d\n", k); */
  /* printf("Residual                   %.15f\n\n", error); */

} 	

