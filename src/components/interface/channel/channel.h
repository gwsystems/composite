/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <cos_types.h>
#include <cos_component.h>

cbuf_t channel_shared_page_alloc(cos_channelkey_t key, vaddr_t *pgaddr);
cbuf_t channel_shared_page_allocn(cos_channelkey_t key, unsigned long num_pages, vaddr_t *pgaddr);
cbuf_t channel_shared_page_map(cos_channelkey_t key, vaddr_t *pgaddr, unsigned long *num_pages);

#endif /* CHANNEL_H */
