#include <stdio.h>

int main()
{
	void *p = 0;
	printf("%p %p\n", p, p + 1);
	return 0;
}
