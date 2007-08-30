#include <stdio.h>

#define rdtscll(val) \
      __asm__ __volatile__("rdtsc" : "=A" (val))

class base {
public:
  base(void) {};
  ~base(void) {};
  virtual int blah(base *b) { return b->foo(); }
  virtual int foo(void) { return 0; }
};

class child1 : public base {
public:
  child1(void) {};
  ~child1(void) {};
  int blah(void) { return 1; }
  int foo(void) { return 1; }
};

class child2 : public base {
public:
  child2(void) {};
  ~child2(void) {};
  virtual int blah(void) { return 2; }
  virtual int foo(void) { return 2; }
};

class child3 : public child2 {
public:
  child3(void) {};
  ~child3(void) {};
  int blah(void) { return 3; }
  int foo(void) { return 3; }
};

static int fn(void) __attribute__((noinline));
static int fn(void) {
  return 0;
} 

int (*fnptr)(void) = fn;

#define ITER 10000
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

  test(c1);
  test(b);
  test(c3);
  
  return 0;
}
