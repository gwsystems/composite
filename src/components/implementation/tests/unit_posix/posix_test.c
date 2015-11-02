#include <stdio.h>

#define RUN_TEST(a) { \
extern int test_ ##a (void); \
int e = test_ ##a (); \
if (e) printf("%s test failed, %d error(s)\n", #a, e); \
else   printf("%s test passed\n", #a); \
err += e; \
}

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#define UHZ 2890

int
run_bench(const char *label, size_t (*bench)(void *), void *params)
{
	unsigned long long int start, end;
	
	puts(label);
	rdtscll(start);
	bench(params);
	rdtscll(end);
	printf("time %d %dus\n", (int)(end-start), (int)((end-start)/UHZ));
}

#define RUN(a, b) \
	extern size_t (a)(void *); \
	run_bench(#a " (" #b ")", (a), (b))

void
cos_init(void *args)
{
	int err=0;

	printf("==========libc test=========\n");

	RUN_TEST(fnmatch);
	RUN_TEST(malloc);
	RUN_TEST(memstream);
	RUN_TEST(qsort);
	RUN_TEST(sscanf);
	RUN_TEST(snprintf);
	RUN_TEST(string);
	RUN_TEST(strtod);
	RUN_TEST(strtol);
	RUN_TEST(wcstol);

	printf("total errors: %d\n", err);
	printf("==========libc bench=========\n");

	RUN(b_malloc_sparse, 0);
	RUN(b_malloc_bubble, 0);
	RUN(b_malloc_tiny1, 0);
	RUN(b_malloc_tiny2, 0);
	RUN(b_malloc_big1, 0);
	RUN(b_malloc_big2, 0);

	RUN(b_string_strstr, "abcdefghijklmnopqrstuvwxyz");
	RUN(b_string_strstr, "azbycxdwevfugthsirjqkplomn");
	RUN(b_string_strstr, "aaaaaaaaaaaaaacccccccccccc");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_memset, 0);
	RUN(b_string_strchr, 0);
	RUN(b_string_strlen, 0);

	RUN(b_regex_compile, "(a|b|c)*d*b");
	RUN(b_regex_search, "(a|b|c)*d*b");
	RUN(b_regex_search, "a{25}b");

	printf("=============DONE!==========\n");

	return;
}
