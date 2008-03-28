#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 45

char foo[MAX_LEN];

void print_mpd(int state) 
{
	char *pd = "\n\nSeparate Protection Domains:\n";
	char *ni = "\n\nSingle Protection Domain:\n";
	char *str = state ? pd: ni;
	
	write(1, str, strlen(str));
}

int print(int len_ptr)
{
	char *string = cos_get_arg_region();
//	int *foo = cos_get_arg_region();
	int ret;

//	ret = print_vals(*foo, len_ptr, 0, 0);
	snprintf(foo, MAX_LEN, "%s", string);
	ret = write(1, foo, len_ptr);
	
	return ret;
}

static int nothing = 0;
int print_vals(int val1, int val2, int val3, int val4)
{
	char str[MAX_LEN];
	int ret;

	snprintf(str, MAX_LEN, "%d:\t%d\t%d\t%d\t%d\n", cos_get_thd_id(), val1, val2, val3, val4);
	ret = write(1, str, strlen(str));
	nothing++;

	return ret; 
}

int main(void) { return 0; }
