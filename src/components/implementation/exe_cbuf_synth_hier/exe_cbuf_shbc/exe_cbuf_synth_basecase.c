#include <cos_component.h>
#include <print.h>
#include <cbuf.h>


#define AVG_INVC_CYCS 1000   /* for each invocation cost, for this machnes is 934 */
volatile unsigned long kkk = 0;

//long sum = 0;
unsigned long left(unsigned long execution_t, unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
//	if (cos_get_thd_id() == 14) printc("thd 14 base comp, exe time left:%lu\n", execution_t);

	//return execution_t > AVG_INVC_CYCS ? execution_t - AVG_INVC_CYCS : 0;
	unsigned long i;
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 15 * 2;
	for (i=0;i < left;i++) kkk++;  

	/* printc("\n ~~thd %ld return from bottom %d (left)~~\n", cos_get_thd_id(), cos_spd_id()); */
	/* printc("total bottom %d\n",sum++); */
	return 0;
}

unsigned long right(unsigned long execution_t, unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
//	if (cos_get_thd_id() == 14) printc("thd 14 base comp, exe time left:%lu\n", execution_t);
	//return execution_t > AVG_INVC_CYCS ? execution_t - AVG_INVC_CYCS : 0;
	unsigned long i;
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 15 * 2;
	for (i=0;i < left;i++) kkk++;  

	/* printc("\n thd %ld return from bottom %d (right)~~\n", cos_get_thd_id(), cos_spd_id()); */
	/* printc("total bottom %d\n",sum++); */
	return 0;
}
