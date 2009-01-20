#include <stdio.h>


#include <sched.h>
void set_prio(void)
{
	struct sched_param sp;

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		printf("\n");
	}
	sp.sched_priority = 99;
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: "); printf("\n");
	}

	return;
}

#define rdtscll(val) \
      __asm__ __volatile__("rdtsc" : "=A" (val))

class base {
public:
  base(void) {};
  ~base(void) {};
  virtual int blah(base *b) { return 0; }
  virtual void foo(void) { return; }
};

class child1 : public base {
public:
  child1(void) {};
  ~child1(void) {};
  int blah(void) { return 1; }
  void foo(void) { return; }
};

class child2 : public base {
public:
  child2(void) {};
  ~child2(void) {};
  virtual int blah(void) { return 2; }
  virtual void foo(void) { return; }
};

class child3 : public child2 {
public:
  child3(void) {};
  ~child3(void) {};
  virtual int blah(void) { return 3; }
  virtual void foo(void) { return; }
};

static void fn(void) __attribute__((noinline));
static void fn(void) {
  return;
} 

void (*fnptr)(void) = fn;

#define ITER 100000
void test(base *b) {
  int iter = ITER;
  unsigned long long start, end;

  b->blah(b);
  b->foo();

  rdtscll(start);
  for (iter = 0; iter < ITER ; iter++) {
    b->foo();//blah(b);
  }
  rdtscll(end);

  printf("Virtual function call takes %lld.\n", (end-start)/ITER);

  rdtscll(start);
  for (iter = 0; iter < ITER ; iter++) {
    fn();
  }
  rdtscll(end);

  printf("Function call takes %lld.\n", (end-start)/ITER);

  rdtscll(start);
  for (iter = 0; iter < ITER ; iter++) {
    (*fnptr)();
  }
  rdtscll(end);

  printf("Function pointer call takes %lld.\n\n", (end-start)/ITER);
  
  return;
}

int main(void)
{
  base *b = new base();
  base *c1 = new child1();
  base *c3 = new child3();
  printf("%d\t%d\t%d\n", b->blah(b), dynamic_cast<base*>(c1)->blah(c1), c3->blah(c1));

  set_prio();

  printf("Base class:\n");
  test(b);
  printf("Non-virtual class\n");
  test(c1);
  printf("Virtual fns in class\n");
  test(c3);
  
  return 0;
}
