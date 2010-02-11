/**
 * Copyright 2010 by Gabriel Parmer and The George Washington
 * University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cobj_format.h>
#include <string.h>

struct cobj_sect *cobj_sect_get(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	if (h->nsect <= sect_id) return NULL;
	s = (struct cobj_sect*)&h[1];

	return &s[sect_id];
}

struct cobj_symb *cobj_symb_get(struct cobj_header *h, unsigned int symb_id)
{
	struct cobj_symb *s;

	if (symb_id >= h->nsymb) return NULL;
	s = (struct cobj_symb*)&cobj_sect_get(h, h->nsect-1)[1];
	
	return &s[symb_id];
}

struct cobj_cap *cobj_cap_get(struct cobj_header *h, unsigned int cap_id)
{
	struct cobj_cap *c;

	if (cap_id >= h->ncap) return NULL;
	c = (struct cobj_cap*)&cobj_symb_get(h, h->nsymb-1)[1];

	return &c[cap_id];
}

char *cobj_sect_contents(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return NULL;
//	assert(s->offset);

	return ((char *)h) + s->offset;
}

u32_t cobj_sect_size(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return 0; 
//	assert(s->offset);

	return s->bytes;
}

struct cobj_header *cobj_create(u32_t id, u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap, 
				char *space, unsigned int sz)
{
	struct cobj_header *h = (struct cobj_header*)space;
	u32_t tot_sz = 0;
	const unsigned int sect_symb_cap_sz = 
		nsect * sizeof(struct cobj_sect) + 
		nsymb * sizeof(struct cobj_symb) + 
		ncap * sizeof(struct cobj_cap);

	if (nsect != COBJ_NSECT) return NULL;
	tot_sz = sect_sz + sizeof(struct cobj_header) + sect_symb_cap_sz;
	if (tot_sz > sz) return NULL;

	h->id = id;
	h->nsect = nsect;
	h->nsymb = nsymb;
	h->ncap = ncap;
	h->size = tot_sz;
	
	memset(&h[1], 0, sect_symb_cap_sz);

	return h;
}

u32_t cobj_size_req(u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap)
{
	return  sect_sz +
		sizeof(struct cobj_header) +
		nsect * sizeof(struct cobj_sect) + 
		nsymb * sizeof(struct cobj_symb) + 
		ncap * sizeof(struct cobj_cap);
}

int cobj_sect_init(struct cobj_header *h, unsigned int sect_idx, u32_t flags, u32_t vaddr, u32_t size)
{
	struct cobj_sect *s;
	u32_t offset;

	if (sect_idx >= h->nsect) return -1;

	if (sect_idx == 0) {
		offset = (u32_t)(&cobj_cap_get(h, h->ncap-1)[1]);
	} else {
		s = cobj_sect_get(h, sect_idx-1);
		if (s->flags & COBJ_SECT_UNINIT) return -1;
		offset = s->offset + s->bytes;
	}
	if (offset + size > h->size) return -1;

	s = cobj_sect_get(h, sect_idx);
	s->offset = offset;
	s->bytes = size;
	s->vaddr = vaddr;
	s->flags = flags;

	return 0;
}

int cobj_symb_init(struct cobj_header *h, unsigned int symb_idx, u32_t type, u32_t vaddr)
{
	struct cobj_symb *s;

	s = cobj_symb_get(h, symb_idx);
	if (!s) return -1;
	s->type = type;
	s->vaddr = vaddr;

	return 0;
}

int cobj_cap_init(struct cobj_header *h, unsigned int cap_idx, u32_t cap_off, 
		  u32_t dest_id, u32_t sfn, u32_t cstub, u32_t sstub)
{
	struct cobj_cap *c;

	c = cobj_cap_get(h, cap_idx);
	if (!c) return -1;
	c->dest_id = dest_id;
	c->sfn = sfn;
	c->cstub = cstub;
	c->sstub= sstub;
	c->cap_off = cap_off;

	return 0;
}
