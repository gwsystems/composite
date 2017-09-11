/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

/* 
 * This is a place-holder for when all of IP is moved here from
 * cos_net.  There should really be no performance difference between
 * having this empty like this, and having IP functionality in here.
 * If anything, due to cache (TLB) effects, having functionality in
 * here will be slower.
 */
#include <cos_component.h>

#include <net_internet.h>

#include <net_if.h>

/* required so that we can have a rodata section */
const char *name = "cos_ip";

int ip_xmit(spdid_t spdid, struct cos_array *d)
{
	return netif_event_xmit(cos_spd_id(), d);
}

int ip_wait(spdid_t spdid, struct cos_array *d)
{
	return netif_event_wait(cos_spd_id(), d);
}

int ip_netif_release(spdid_t spdid)
{
	return netif_event_release(cos_spd_id());
}

int ip_netif_create(spdid_t spdid)
{
	return netif_event_create(cos_spd_id());
}
