#ifndef CRINGBUF_H
#define CRINGBUF_H

/* Include alloc.h before you include this file */

/* shared ring buffer*/
struct __cringbuf {
	int head, tail;
	char buffer[0];
};
/* local data-structure */
struct cringbuf {
	int sz; 		/* total size of the mapping */
	struct __cringbuf *b;
};

/* Code in progress: */
/* static inline void  */
/* cringbuf_init(struct cringbuf *rb, void *buffer, int alloc_sz) */
/* { */
/* 	assert(rb && buffer); */
/* 	rb->b = buffer; */
/* 	rb->sz = alloc_sz - sizeof(struct __cringbuf); */
/* 	rb->b->head = rb->b->tail = 0; */
/* } */

/* static inline int  */
/* cringbuf_empty(struct cringbuf *rb) */
/* {  */
/* 	assert(rb && rb->b); */
/* 	return rb->b->head == rb->b->tail;  */
/* } */

/* static inline int  */
/* cringbuf_sz(struct cringbuf *rb) */
/* { */
/* 	int head, tail; */

/* 	assert(rb); */
/* 	head = rb->b->head; */
/* 	tail = rb->b->tail; */
/* 	if (head <= tail) return tail-head; */
/* 	else              return rb->sz - (head-tail); */
/* } */

/* static inline int  */
/* cringbuf_full(struct cringbuf *rb) */
/* {  */
/* 	struct __cringbuf *__rb; */

/* 	assert(rb && rb->b); */
/* 	return cringbuf_sz(rb) == rb->sz; */
/* } */

/* /\* returns a contiguous extent of active entries (not _all_ used entries) *\/ */
/* static inline char * */
/* cringbuf_active_extent(struct cringbuf *rb, int *len, int amnt) */
/* { */
/* 	struct __cringbuf *b; */
/* 	int head, tail; */

/* 	assert(rb && rb->b); */
/* 	b = rb->b; */
/* 	if (cringbuf_empty(rb)) return NULL; */
/* 	head = b->head; */
/* 	tail = b->tail; */
/* 	assert(head != tail); */
/* 	if (head < tail) *len = tail - head; */
/* 	else             *len = rb->sz - head; */
/* 	if (*len > amnt) *len = amnt; */

/* 	return b->buffer[head]; */
/* } */

/* /\* amnt should be < cringbuf_active_extent *\/ */
/* static inline void */
/* cringbuf_consume(struct cringbuf *rb, int amnt) */
/* { */
/* 	assume(rb && rb->b); */

	
/* } */

#endif
