#ifndef   	MEM_MGR_H
#define   	MEM_MGR_H

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags);
void mman_release_page(spdid_t spd, vaddr_t addr, int flags);
vaddr_t mman_alias_page(spdid_t d_spd, vaddr_t d_addr, spdid_t s_spd, vaddr_t s_addr);
void mman_print_stats(void);

#endif 	    /* !MEM_MGR_H */
