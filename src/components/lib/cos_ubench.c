#include <cos_ubench.h>
#include <stdio.h>
#include <llprint.h>

int
_expect_llu(int predicate, char *str, long long unsigned a,
            long long unsigned b, char *errcmp, char *testname, char * file, int line)
{
        if (predicate) {
                PRINTC("Failure: %s %s @ %d: ",
                        testname, file, line);
                printc("%s %lld %s %lld)\n", str, a, errcmp, b);
                return -1;
        }
        return 0;
}

int
_expect_ll(int predicate, char *str, long long a,
           long long b, char *errcmp, char *testname, char * file, int line)
{
        if (predicate) {
                PRINTC("Failure: %s %s @ %d: ",
                        testname, file, line);
                printc("%s %lld %s %lld)\n", str, a, errcmp, b);
                return -1;
        }
        return 0;
}

