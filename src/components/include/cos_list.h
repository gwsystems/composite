#ifndef COS_LIST_H
#define COS_LIST_H

#define STATIC_INIT_LIST(obj, next, prev) obj = {.next = &obj, .prev = &obj}

#define INIT_LIST(obj, next, prev)                 \
	do {                                       \
		(obj)->next = (obj)->prev = (obj); \
	} while (0)

#define ADD_LIST(head, new, next, prev)           \
	do {                                      \
		(new)->next       = (head)->next; \
		(new)->prev       = (head);       \
		(head)->next      = (new);        \
		(new)->next->prev = (new);        \
	} while (0)

#define APPEND_LIST(last, head, next, prev)        \
	do {                                       \
		(last)->next->prev = (head)->prev; \
		(head)->prev->next = (last)->next; \
		(last)->next       = (head);       \
		(head)->prev       = (last);       \
	} while (0)

#define REM_LIST(obj, next, prev)                  \
	do {                                       \
		(obj)->next->prev = (obj)->prev;   \
		(obj)->prev->next = (obj)->next;   \
		(obj)->next = (obj)->prev = (obj); \
	} while (0)

#define FIRST_LIST(obj, next, prev) ((obj)->next)

#define LAST_LIST(obj, next, prev) ((obj)->prev)

#define EMPTY_LIST(obj, next, prev) ((obj)->next == (obj))

#define ADD_END_LIST(head, new, next, prev) ADD_LIST(LAST_LIST(head, next, prev), new, next, prev)

#endif
