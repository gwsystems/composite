#ifndef COS_LIST_H
#define COS_LIST_H

#define STATIC_INIT_LIST(obj, next, prev)         \
	obj = {                                   \
		.next = &obj,			  \
		.prev = &obj			  \
	}
	      
#define INIT_LIST(obj, next, prev)		  \
	(obj)->next = (obj)->prev = (obj)

#define ADD_LIST(head, new, next, prev) 	  \
	(new)->next = (head)->next;		  \
	(new)->prev = (head);			  \
	(head)->next = (new);			  \
	(new)->next->prev = (new)

#define REM_LIST(obj, next, prev)                 \
	(obj)->next->prev = (obj)->prev;	  \
	(obj)->prev->next = (obj)->next;	  \
	(obj)->next = (obj)->prev = (obj)

#define FIRST_LIST(obj, next, prev)               \
	((obj)->next)

#define LAST_LIST(obj, next, prev)                \
	((obj)->prev)

#define EMPTY_LIST(obj, next, prev)		  \
	((obj)->next == (obj))


/* 
 * create list_add_type and list_rem_type where type is the type of
 * structure we are accessing, qual is an optional qualification to
 * differentiate between two lists in a given structure, l_ptr_name is
 * the variable name in the struct of type for the list_ptr_t pointer.
 * E.g. when type is "node" and qual is "":
 *
 * l_add_node(struct node *to, struct node *new);
 * l_rem_node(struct node *rem);
 */

typedef struct list_ptr {
	void *next, *prev;
} list_ptr_t;

#define LIST_OPS_CREATE(type, qual, l_name)                \
void l_add_##type##qual(struct type *to, struct type *new) \
{                                                          \
	struct type *tmp;                                  \
	tmp = (type)to->l_name.next;		           \
	tmp->l_name.prev = new;                            \
	to->l_name.next = new;                             \
	new->l_name.prev = to;                             \
	new->l_name.next = tmp;                            \
}                                                          \
void l_rem_##type##qual(struct type *rem)                  \
{                                                          \
	rem->l_name.next->l_name.prev = rem->l_name.prev;  \
	rem->l_name.prev->l_name.next = rem->l_name.next;  \
	rem->l_name.next = rem->l_name.prev = rem;         \
}

  /*
struct type *iterator, *start;
for (iterator = head, start = head ; 
     ; 
     iterator = (struct type *)iterator->l_name.next) {
	
}
  */
 
#endif
