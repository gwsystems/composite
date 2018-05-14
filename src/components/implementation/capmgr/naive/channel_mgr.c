/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <channel.h>
#include <cap_info.h>

cbuf_t
channel_shared_page_allocn_cserialized(vaddr_t *pgaddr, int *unused, cos_channelkey_t key, unsigned long npages)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	cbuf_t shmid = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) goto done;
	if (!cur_shi) goto done;

	shmid = cap_shmem_region_alloc(cur_shi, key, npages);
	if (!shmid) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, shmid);

done:
	return shmid;
}

cbuf_t
channel_shared_page_map_cserialized(vaddr_t *pgaddr, unsigned long *num_pages, cos_channelkey_t key)
{
	spdid_t cur = cos_inv_token();
	struct cap_comp_info  *cur_rci = cap_info_comp_find(cur);
	struct cap_shmem_info *cur_shi = cap_info_shmem_info(cur_rci);
	cbuf_t id = 0;

	if (!cur_rci || !cap_info_init_check(cur_rci)) return 0;
	if (!cur_shi) return 0;

	id = cap_shmem_region_map(cur_shi, 0, key, num_pages);
	if (id == 0 || *num_pages == 0) goto done;

	*pgaddr = cap_shmem_region_vaddr(cur_shi, id);
	assert(*pgaddr);

done:
	return id;
}
