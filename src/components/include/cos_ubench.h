#ifndef COS_UBENCH_H
#define COS_UBENCH_H

#include <cos_types.h>
#include <cos_debug.h>
#include <llprint.h>

#undef EXPECT_LLU
#define EXPECT_LLU(boole ,printstr, errcmp, a, b, name) \
	_expect_llu((boole), (": " #a " " errcmp " " #b " (evaluated to"), a, b, errcmp, name, __FILE__, __LINE__)

#define EXPECT_LLU_NEQ(a, b, name) EXPECT_LLU((a) != (b), "%llu",  "==", (a), (b), name)

#define EXPECT_LLU_LT(a, b, name) EXPECT_LLU((a) > (b), "%llu",  "<=", (a), (b), name)

#undef EXPECT_LL
#define EXPECT_LL(boole ,printstr, errcmp, a, b, name) \
	_expect_ll((boole), (": " #a " " errcmp " " #b " (evaluated to"), a, b, errcmp, name, __FILE__, __LINE__)

#define EXPECT_LL_NEQ(a, b, name) EXPECT_LL((a) != (b), "%lld",  "==", (a), (b), name)

#define EXPECT_LL_EQ(a, b, name) EXPECT_LL((a) == (b), "%lld",  "!=", (a), (b), name)

#define EXPECT_LL_LT(a, b, name) EXPECT_LL((a) > (b), "%lld",  "<=", (a), (b), name)

int
_expect_llu(int predicate, char *str, long long unsigned a,
            long long unsigned b, char *errcmp, char *testname, char * file, int line);
int
_expect_ll(int predicate, char *str, long long a,
           long long b, char *errcmp, char *testname, char * file, int line);

#endif /* COS_UBENCH_H */
