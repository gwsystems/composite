#include <cos_component.h>
#include <print.h>
#include <cbuf.h>


#define AVG_INVC_CYCS 1000   /* for each invocation cost, for this machnes is 934 */
volatile unsigned long kkk = 0;

#define I7
//long sum = 0;
unsigned long left(unsigned long execution_t, unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
	/* return execution_t > AVG_INVC_CYCS ? execution_t - AVG_INVC_CYCS : 0; */
	unsigned long i;
	if (execution_t <= AVG_INVC_CYCS ) return 0;
#ifdef I7
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 15 * 2;
#else
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 6;
#endif

	for (i=0;i < left;i++) kkk++;
	return 0;
}

unsigned long right(unsigned long execution_t, unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
	/* return execution_t > AVG_INVC_CYCS ? execution_t - AVG_INVC_CYCS : 0; */
	unsigned long i;
	if (execution_t <= AVG_INVC_CYCS ) return 0;

#ifdef I7
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 15 * 2;
#else
	unsigned long left = (execution_t - AVG_INVC_CYCS) / 6;
#endif

	for (i=0;i < left;i++) kkk++;
	return 0;
}
