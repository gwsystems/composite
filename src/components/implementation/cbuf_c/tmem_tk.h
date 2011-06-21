/* Toolkit of transient memory manager */

/* Interfaces of tmem_base and tmem_mgr */

#ifndef TMEM_TK_H
#define TMEM_TK_H

#include <tmem_conf.h>

/* some static inline functions are implemented in tmem.h */

/* implemented in transien mem base and need to call manager */
inline struct spd_stk_info * get_spd_info(spdid_t spdid);
int tmem_wait_for_mem_no_dependency(struct spd_stk_info *ssi);
int tmem_wait_for_mem(struct spd_stk_info *ssi);
inline int tmem_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare);
void return_tmem(struct spd_stk_info *ssi);
tmem_item * tmem_grant(struct spd_stk_info *ssi);

/* need to be implemented in manager */
void mgr_map_client_mem(tmem_item *tmi, struct spd_stk_info *info);
tmem_item * mgr_get_client_mem(struct spd_stk_info *ssi);
void spd_mark_relinquish(spdid_t spdid);
void spd_unmark_relinquish(struct spd_stk_info *ssi);
u32_t resolve_dependency(struct spd_stk_info *ssi, int skip_stk);

#endif
