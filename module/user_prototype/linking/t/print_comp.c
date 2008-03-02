#include <cos_component.h>

#include <stdio.h>
#include <string.h>

int print(void)
{
	int ret = 5, *len_ptr;
	char *string;

	len_ptr = COS_FIRST_ARG;
	string = (char *)(len_ptr + 1); // plus 4 bytes 

	ret = write(1, string, *len_ptr);
	
	return ret;
}

#define MAX_LEN 45
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
