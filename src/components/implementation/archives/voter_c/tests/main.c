#include <cos_debug.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>

void
my_entry_pt(void *p)
{
  while(1){
    printc("thd3\n");
    sl_thd_yield(sl_thdid());
  }
  assert(0);
}
void
my_entry_pt_2(void *p)
{
    while(1){ 
      printc("thd2\n");
      sl_thd_yield(sl_thdid());
    }   
    assert(0);
}
void 
cos_init(void) 
{
  printc("%s","heyo");
 
  struct sl_thd *thread_a = sl_thd_alloc(my_entry_pt,NULL); 
  sl_thd_param_set(thread_a, sched_param_pack(SCHEDP_PRIO, 2));
  
  struct sl_thd *thread_b = sl_thd_alloc(my_entry_pt_2,NULL); 
  sl_thd_param_set(thread_b, sched_param_pack(SCHEDP_PRIO, 2));

  sl_sched_loop();
}

