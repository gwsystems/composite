/*
 *
 * Simple vm list.. static alloc of nodes.. O(1) insert, delete..
 * + O(1) insert any, delete any : Interesting data structure??? 
 *    nodes are allocated from a static array.. 
 *    so I know what I want to insert or delete: by indexing, and
 *    therefore, inserting or deleting any node, is O(1)
 */

#ifndef VK_API_H
#define VK_API_H

#include "vk_types.h"

struct vm_node {
	int id;
	struct vm_node *next, *prev;
};

struct vm_list {
	struct vm_node *s;
}; 

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
struct vm_list vms_runqueue, vms_expended, vms_exit;
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
struct vm_list vms_under, vms_over, vms_boost, vms_wait, vms_exit;
#endif

struct vm_node vmnode[COS_VIRT_MACH_COUNT];

static struct vm_node *
vm_next(struct vm_list *l)
{
	struct vm_node *p;
	assert(l);

	p = l->s;
	if (p == NULL) return NULL;
	if (p->next == l->s) return p;
	l->s = p->next;

	return p;
}

static struct vm_node *
vm_prev(struct vm_list *l)
{
	struct vm_node *p;
	assert(l);

	p = l->s;
	if (p == NULL) return NULL;
	if (p->next == l->s) return p;
	l->s = p->prev;

	return p;
}

static struct vm_node *
vm_deletenode(struct vm_list *l, struct vm_node *n)
{
	assert(l && n);

	if (l->s == n && n->next == n) {
		l->s = NULL;
		return n;
	}
	if (l->s == n) l->s = n->next;

	n->prev->next = n->next;
	n->next->prev = n->prev;
	n->next = n->prev = n;

	return n;
}

static void
vm_insertnode(struct vm_list *l, struct vm_node *n)
{
	struct vm_node *p;

	assert(l && n);

	if (l->s == NULL) {
		l->s = n;
		return ;
	}

	p = l->s;
	n->next = p;
	n->prev = p->prev;

	p->prev->next = n;
	p->prev = n;
}

static void
vm_list_init(void)
{
	int i;

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	vms_runqueue.s = vms_exit.s = vms_expended.s = NULL;
	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
		vmnode[i].id = i;
		vmnode[i].prev = vmnode[i].next = &vmnode[i];
		vm_insertnode(&vms_runqueue, &vmnode[i]); 
	}
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	vms_under.s = vms_over.s = vms_boost.s = vms_wait.s = vms_exit.s = NULL;
	for (i = 0 ; i < COS_VIRT_MACH_COUNT ; i ++) {
		vmnode[i].id = i;
		vmnode[i].prev = vmnode[i].next = &vmnode[i];
		vm_insertnode(&vms_under, &vmnode[i]); 
	}
#endif

}

#endif /* VK_API_H */
