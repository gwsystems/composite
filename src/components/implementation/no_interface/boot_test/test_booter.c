#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include "boot_deps.h"

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }


int
prints(char *s)
{
		int len = strlen(s);

		cos_llprint(s, len);

		return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
  	  va_list arg_ptr;
  	  int ret, len = 128;

  	  va_start(arg_ptr, fmt);
  	  ret = vsnprintf(s, len, fmt, arg_ptr);
  	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

void 
cos_init(void)
{
	
	prints("\n|*****************************|\n");
	prints(" Wecome to test_boot component!\n");
	prints("|*****************************|\n");
	sinvcap_t sinv = 0;
	compcap_t cc;
	int sinv_ret = 0;
//	sinv_ret = cos_sinv(BOOT_SINV_CAP);

}
