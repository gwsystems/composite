#include <omp.h>
#include <stdio.h>

#define SIZE 96

#define MEAS_ITER 100

#define NCPU 39

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

/*************************************************************************
  This program is part of the
	OpenMP Source Code Repository

	http://www.pcg.ull.es/ompscr/
	e-mail: ompscr@etsii.ull.es

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License 
  (LICENSE file) along with this program; if not, write to
  the Free Software Foundation, Inc., 59 Temple Place, Suite 330, 
  Boston, MA  02111-1307  USA
	
FILE:		c_lu.c
VERSION:	1.0
DATE:
AUTHOR:		Arturo González-Escribano
COMMENTS TO:	arturo@infor.uva.es
DESCRIPTION:       
		LU reduction of a 2D dense matrix

COMMENTS:        
REFERENCES:     
BASIC PRAGMAS:	parallel-for (stride scheduling)
USAGE: 		./c_lu.par <size>
INPUT:		The matrix has fixed innitial values: 
		M[i][j] = 1.0	                   iff (i==j)
		M[i][j] = 1.0 + (i*numColums)+j    iff (i!=j)

OUTPUT:		Compile with -DDEBUG to see final matrix values
FILE FORMATS:
RESTRICTIONS:
REVISION HISTORY:
**************************************************************************/

//#include<OmpSCR.h>


/* PROTOYPES */
void lu(int, int);

unsigned long long cost[MEAS_ITER];
double **M_glb;
double **L_glb;

/* MAIN: PROCESS PARAMETERS */
int main(int argc, char *argv[]) {
	int nthreads, size;
	char *argNames[1] = { "size" };
	char *defaultValues[1] = { "500" };
	char *timerNames[1] = { "EXE_TIME" };

#ifdef DISABLE
	return 0;
#endif

#ifndef COS
	printf("Setting highest prio...\n");
	set_prio();
	set_smp_affinity();
	//set_openmp_affinity();
	
	printf("Done.\n");
	printf("getenv %s, %s\n", getenv("GOMP_CPU_AFFINITY"),getenv("OMP_WAIT_POLICY"));
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
	nthreads = omp_get_max_threads();
/* OSCR_init( nthreads, */
/* 	"LU reduction of a dense matrix.", */
/* 	NULL, */
/* 	1, */
/* 	argNames, */
/* 	defaultValues, */
/* 	1, */
/* 	1, */
/* 	timerNames, */
/* 	argc, */
/* 	argv ); */

/* 1. GET PARAMETERS */
	size = SIZE;//OSCR_getarg_int(1);

/* 2. CALL COMPUTATION */
	int i, j;
	unsigned long long s, e;
/* 0. ALLOCATE MATRICES MEMORY */
	M_glb = (double **)OSCR_calloc(size, sizeof(double *));
	L_glb = (double **)OSCR_calloc(size, sizeof(double *));
	for (i=0; i<size; i++) {
		M_glb[i] = (double *)OSCR_calloc(size, sizeof(double));
		L_glb[i] = (double *)OSCR_calloc(size, sizeof(double));
	}

	for (i = 0; i < MEAS_ITER; i++) {
		rdtscll(s);
		lu(nthreads, size);
		rdtscll(e);
		cost[i] = e - s;
		for (j=0; j<size; j++) {//zero out
			memset(M_glb[j], 0, size*sizeof(double));
			memset(L_glb[j], 0, size*sizeof(double));
		}
	}

	for (i = 0; i < MEAS_ITER; i++) {
		printf("cost %d: %llu\n", i, cost[i]);
	}

/* 3. REPORT */
//OSCR_report();


	return 0;
}


/*
* LU FORWARD REDUCTION
*/
void lu(int nthreads, int size) {
/* DECLARE MATRIX AND ANCILLARY DATA STRUCTURES */
	double **M = M_glb;
	double **L = L_glb;

/* VARIABLES */
	int i,j,k;


/* 1. INITIALIZE MATRIX */
	for (i=0; i<size; i++) {
		for (j=0; j<size; j++) {
			if (i==j) M[i][j]=1.0;
			else M[i][j]=1.0+(i*size)+j; 
			L[i][j]=0.0;
		}
	}

/* 3. START TIMER */
//OSCR_timer_start(0);

/* 4. ITERATIONS LOOP */
	for(k=0; k<size-1; k++) {

		/* 4.1. PROCESS ROWS IN PARALLEL, DISTRIBUTE WITH nthreads STRIDE */
#if NCPU > 1
#pragma omp parallel for default(none) shared(M,L,size,k) private(i,j) schedule(static,1)
#endif
		for (i=k+1; i<size; i++) {
			/* 4.1.1. COMPUTE L COLUMN */
			L[i][k] = M[i][k] / M[k][k];

			/* 4.1.2. COMPUTE M ROW ELEMENTS */
			for (j=k+1; j<size; j++) 
				M[i][j] = M[i][j] - L[i][k]*M[k][j];
		}

/* 4.2. END ITERATIONS LOOP */
	}


/* 5. STOP TIMER */
//OSCR_timer_stop(0);

/* 6. WRITE MATRIX (DEBUG) */
/* #ifdef DEBUG */
/* 	{ */
/* 		int i,j; */

/* /\* 6.1. WRITE M CONTAINING UPPER PART *\/ */
/* 		fprintf(stderr,"Matrix: M -----------------\n"); */
/* 		for (i=0; i<size; i++) { */
/* 			for (j=0; j<size; j++) { */
/* 				fprintf(stderr,"%6.1f\t", M[i][j]); */
/* 			} */
/* 			fprintf(stderr,"\n"); */
/* 		} */

/* /\* 6.2. WRITE L CONTAINING THE LOWER PART *\/ */
/* 		fprintf(stderr,"Matrix: L ----------------------------\n"); */
/* 		for (i=0; i<size; i++) { */
/* 			for (j=0; j<size; j++) { */
/* 				fprintf(stderr,"%6.1f\t", L[i][j]); */
/* 			} */
/* 			fprintf(stderr,"\n"); */
/* 		} */
/* 	} */
/* #endif */

/* 7. END */
}

