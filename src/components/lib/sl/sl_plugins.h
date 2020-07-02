#ifndef SL_PLUGINS_H
#define SL_PLUGINS_H

#include <res_spec.h>
#include <sl_mod_policy.h>

/*
 * The build system has to add in the appropriate backends for the
 * system.  We rely on the linker to hook up all of these function
 * call instead of function pointers so that we can statically analyze
 * stack consumption and execution paths (e.g. for WCET) which are
 * prohibited by function pointers.  Additionally, significant work
 * (by Daniel Lohmann's group) has shown that a statically configured
 * system is more naturally fault resilient.  A minor benefit is the
 * performance of not using function pointers, but that is secondary.
 */
struct sl_thd_policy *sl_thd_alloc_backend(thdid_t tid);
void                  sl_thd_free_backend(struct sl_thd_policy *t);
/*
 * cos_aep_info structs cannot be stack allocated!
 * The thread_alloc_backened needs to provide struct cos_aep_info without
 * any knowledge of the thread being alloced.
 *
 * sl_thd_free_backend to free the cos_aep_info struct
 */
struct cos_aep_info  *sl_thd_alloc_aep_backend(void);

void                  sl_thd_index_add_backend(struct sl_thd_policy *);
void                  sl_thd_index_rem_backend(struct sl_thd_policy *);
struct sl_thd_policy *sl_thd_lookup_backend(thdid_t);
void                  sl_thd_init_backend(void);

/*
 * Each scheduler policy must implement the following API.  See above
 * for why this is not a function-pointer-based API.
 *
 * Scheduler modules (policies) should define the following
 */
struct sl_thd_policy;
static inline struct sl_thd *       sl_mod_thd_get(struct sl_thd_policy *tp);
static inline struct sl_thd_policy *sl_mod_thd_policy_get(struct sl_thd *t);

void                  sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles);
struct sl_thd_policy *sl_mod_schedule(void);

void sl_mod_block(struct sl_thd_policy *t);
void sl_mod_wakeup(struct sl_thd_policy *t);
void sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *tp);

void sl_mod_thd_create(struct sl_thd_policy *t);
void sl_mod_thd_delete(struct sl_thd_policy *t);
void sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int val);
void sl_mod_init(void);

#endif /* SL_PLUGINS_H */
