#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define LINUX_TEST
#include <clist.h>

struct list_test {
	int          id;
	char         filler[2];
	struct clist CLIST_DEF_NAME;
	int          nothingelse;
};

void
print(struct clist_head *lh)
{
	struct list_test *l;

	clist_head_fst(lh, &l);
	printf("[ ");
	for (clist_head_fst(lh, &l); !clist_is_head(lh, l); l = clist_next(l)) {
		printf("%d ", l->id);
	}
	printf("]\n");
}

int
check(struct clist_head *lh, char chk[])
{
	struct list_test *l;
	int               i;

	clist_head_fst(lh, &l);
	for (i = 0, clist_head_fst(lh, &l); !clist_is_head(lh, l); l = clist_next(l), i++) {
		int x = chk[i] - 48;
		if (x != l->id) return 0;
	}
	return 1;
}

int
main(void)
{
	struct clist_head h;
	struct list_test  l0, l1, l2;

	clist_head_init(&h);
	l0.id = 0;
	l1.id = 1;
	l2.id = 2;
	clist_init(&l0);
	clist_init(&l1);
	clist_init(&l2);
	clist_head_add(&h, &l0);
	clist_head_add(&h, &l1);
	clist_head_add(&h, &l2);
	assert(check(&h, "210"));
	clist_rem(&l1);
	assert(clist_singleton(&l1));
	clist_head_add(&h, &l1);
	assert(check(&h, "120"));
	clist_rem(&l1);
	clist_head_append(&h, &l1);
	assert(check(&h, "201"));
	clist_rem(&l1);
	clist_add(&l2, &l1);
	assert(check(&h, "210"));
	clist_rem(&l0);
	clist_rem(&l2);
	clist_rem(&l1);
	assert(check(&h, ""));
	assert(clist_head_empty(&h));

	return 0;
}
