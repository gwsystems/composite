/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <channel.h>

cbuf_t channel_shared_page_allocn_cserialized(vaddr_t *pgaddr, int *unused, cos_channelkey_t key, unsigned long num_pages);
cbuf_t channel_shared_page_map_cserialized(vaddr_t *pgaddr, unsigned long *num_pages, cos_channelkey_t key);

cbuf_t
channel_shared_page_allocn(cos_channelkey_t key, unsigned long num_pages, vaddr_t *pgaddr)
{
	int unused = 0;

	return channel_shared_page_allocn_cserialized(pgaddr, &unused, key, num_pages);
}

cbuf_t
channel_shared_page_alloc(cos_channelkey_t key, vaddr_t *pgaddr)
{
	return channel_shared_page_allocn(key, 1, pgaddr);
}

cbuf_t
channel_shared_page_map(cos_channelkey_t key, vaddr_t *pgaddr, unsigned long *num_pages)
{
	int unused = 0;

	return channel_shared_page_map_cserialized(pgaddr, num_pages, key);
}
