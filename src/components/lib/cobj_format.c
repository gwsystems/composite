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

#ifdef TESTING
#include <stdio.h>
#endif

struct cobj_sect *
cobj_sect_get(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	if (h->nsect <= sect_id) return NULL;
	s = (struct cobj_sect *)((u32_t)h + sizeof(struct cobj_header));
	// s = (struct cobj_sect*)&h[1];

	return &s[sect_id];
}

struct cobj_symb *
cobj_symb_get(struct cobj_header *h, unsigned int symb_id)
{
	struct cobj_symb *s;

	if (symb_id >= h->nsymb) return NULL;
	s = (struct cobj_symb *)((u32_t)h + sizeof(struct cobj_header) + sizeof(struct cobj_sect) * h->nsect);
	//	s = (struct cobj_symb*)&(cobj_sect_get(h, h->nsect-1)[1]);

	return &s[symb_id];
}

struct cobj_cap *
cobj_cap_get(struct cobj_header *h, unsigned int cap_id)
{
	struct cobj_cap *c;

	if (cap_id >= h->ncap) return NULL;
	c = (struct cobj_cap *)((u32_t)h + sizeof(struct cobj_header) + sizeof(struct cobj_sect) * h->nsect
	                        + sizeof(struct cobj_symb) * h->nsymb);
	// c = (struct cobj_cap*)&(cobj_symb_get(h, h->nsymb-1)[1]);

	return &c[cap_id];
}

void *
cobj_vaddr_get(struct cobj_header *h, u32_t vaddr)
{
	u32_t i;

	for (i = 0; i < h->nsect; i++) {
		struct cobj_sect *s;

		s = cobj_sect_get(h, i);
		if (vaddr < s->vaddr || vaddr >= s->vaddr + s->bytes) continue;
		if (s->flags & COBJ_SECT_ZEROS) return NULL;
		return cobj_sect_contents(h, i) + (vaddr - s->vaddr);
	}
	return NULL;
}

int
cobj_sect_empty(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return -1;

	return s->flags & COBJ_SECT_ZEROS;
}

u32_t
cobj_sect_content_offset(struct cobj_header *h)
{
	return sizeof(struct cobj_header) + sizeof(struct cobj_sect) * h->nsect + sizeof(struct cobj_symb) * h->nsymb
	       + sizeof(struct cobj_cap) * h->ncap;
}

char *
cobj_sect_contents(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	if (cobj_sect_empty(h, sect_id)) return NULL;
	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return NULL;

	return ((char *)h) + s->offset;
}

u32_t
cobj_sect_size(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return 0;

	return s->bytes;
}

u32_t
cobj_sect_addr(struct cobj_header *h, unsigned int sect_id)
{
	struct cobj_sect *s;

	s = cobj_sect_get(h, sect_id);
	if (!s || s->flags & COBJ_SECT_UNINIT) return 0;

	return s->vaddr;
}

struct cobj_header *
cobj_create(u32_t id, char *name, u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap, char *space, unsigned int sz,
            u32_t flags)
{
	struct cobj_header *h                = (struct cobj_header *)space;
	u32_t               tot_sz           = 0;
	const unsigned int  sect_symb_cap_sz = nsect * sizeof(struct cobj_sect) + nsymb * sizeof(struct cobj_symb)
	                                      + ncap * sizeof(struct cobj_cap);

	if (!space) return NULL;
	tot_sz = sect_sz + sizeof(struct cobj_header) + sect_symb_cap_sz;
	if (tot_sz > sz) return NULL;

	h->id = id;
	if (name) {
		h->name[0] = '\0';
		strncpy(h->name, name, COBJ_NAME_SZ - 1);
		h->name[COBJ_NAME_SZ - 1] = '\0';
	}
	h->nsect = nsect;
	h->nsymb = nsymb;
	h->ncap  = ncap;
	h->size  = tot_sz;
	h->flags = flags;

	memset(&h[1], 0, sect_symb_cap_sz);

	return h;
}

u32_t
cobj_size_req(u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap)
{
	return sect_sz + sizeof(struct cobj_header) + nsect * sizeof(struct cobj_sect)
	       + nsymb * sizeof(struct cobj_symb) + ncap * sizeof(struct cobj_cap);
}

int
cobj_sect_init(struct cobj_header *h, unsigned int sect_idx, u32_t flags, u32_t vaddr, u32_t size)
{
	struct cobj_sect *s;
	u32_t             offset;

	if (sect_idx >= h->nsect) return -1;

	if (sect_idx == 0) {
		offset = cobj_sect_content_offset(h);
	} else {
		s = cobj_sect_get(h, sect_idx - 1);
		if (s->flags & COBJ_SECT_UNINIT) return -1;
		offset = (s->flags & COBJ_SECT_ZEROS) ? s->offset : s->offset + s->bytes;
	}
	if (!(flags & COBJ_SECT_ZEROS) && offset + size > h->size) return -1;

	s         = cobj_sect_get(h, sect_idx);
	s->offset = offset;
	s->bytes  = size;
	s->vaddr  = vaddr;
	s->flags  = flags;

	return 0;
}

int
cobj_symb_init(struct cobj_header *h, unsigned int symb_idx, const char *name,u32_t type, u32_t vaddr, u32_t user_caps_offset)
{
	struct cobj_symb *s;

	s = cobj_symb_get(h, symb_idx);
	if (!s) return -1;
	strcpy(s->name, name);
	s->type  = type;
	s->vaddr = vaddr;
	s->user_caps_offset = user_caps_offset;

	return 0;
}

int
cobj_cap_init(struct cobj_header *h, unsigned int cap_idx, u32_t cap_off, u32_t dest_id, u32_t sfn, u32_t cstub,
              u32_t sstub, u32_t fault_num)
{
	struct cobj_cap *c;

	c = cobj_cap_get(h, cap_idx);
	if (!c) return -1;
	c->dest_id   = dest_id;
	c->sfn       = sfn;
	c->cstub     = cstub;
	c->sstub     = sstub;
	c->cap_off   = cap_off;
	c->fault_num = fault_num;

	return 0;
}

#ifdef TESTING
#include <malloc.h>

int
main(void)
{
	u32_t               sz;
	char *              mem;
	struct cobj_header *h;
	struct cobj_sect *  sect;

	printf("sizes: header=%d, sect=%d, symb=%d, cap=%d\n", sizeof(struct cobj_header), sizeof(struct cobj_sect),
	       sizeof(struct cobj_symb), sizeof(struct cobj_cap));
	sz  = cobj_size_req(3, 20, 3, 4);
	mem = malloc(sz);
	h   = cobj_create(0, NULL, 3, 20, 3, 4, mem, sz, 0);
	if (!h) return -1;
	printf("predicted=%d, actual=%d, data off=%d\n", sz, h->size, cobj_sect_content_offset(h));

	if (cobj_sect_init(h, 0, COBJ_SECT_READ, 8, 15) || cobj_sect_init(h, 1, COBJ_SECT_ZEROS, 36, 5)
	    || cobj_sect_init(h, 2, COBJ_SECT_READ | COBJ_SECT_WRITE, 128, 5)) {
		printf("fail\n");
		return -1;
	}

	printf("header: id %d=0, nsect %d=2, nsymb %d=3, ncap %d=4, size %d=%d\n", h->id, h->nsect, h->nsymb, h->ncap,
	       h->size, sz);

	sect = cobj_sect_get(h, 0);
	printf("sect %d (off %d): f=%x, o=%d, s=%d, a=%x\n", 0, (u32_t)sect - (u32_t)h, sect->flags, sect->offset,
	       sect->bytes, sect->vaddr);
	sect = cobj_sect_get(h, 1);
	printf("sect %d (off %d): f=%x, o=%d, s=%d, a=%x\n", 1, (u32_t)sect - (u32_t)h, sect->flags, sect->offset,
	       sect->bytes, sect->vaddr);
	sect = cobj_sect_get(h, 2);
	printf("sect %d (off %d): f=%x, o=%d, s=%d, a=%x\n", 2, (u32_t)sect - (u32_t)h, sect->flags, sect->offset,
	       sect->bytes, sect->vaddr);

	printf("data_offset %x, sect 0 data %x, sect 1 data %x\n", (u32_t)h + cobj_sect_content_offset(h),
	       cobj_sect_contents(h, 0), cobj_sect_contents(h, 1));

	return 0;
}
#endif
