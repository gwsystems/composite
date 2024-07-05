#ifndef HEAP_H
#define HEAP_H

#include <cos_component.h>
#include <cos_debug.h>
#include <string.h>

//#undef HEAP_TRACE_DEBUG
#ifdef HEAP_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

struct hentry {
	int index, value;
};

struct heap {                                                  
    int e, max_sz;                                                    
    void **data;                                                      
}; 


static inline void heap_init(struct heap *h, int max_sz);      
static inline void heap_destroy(struct heap *h);               
static inline int  heap_size(struct heap *h);                  
static inline int  heap_empty(struct heap *h);                 
static inline void * heap_peek(struct heap *h);                
static inline struct heap * heap_alloc(int max_sz);            

// Macro to generate type-specific heap structures and functions
#define DECLARE_HEAP(NAME, CMP_FN, UPDATE_FN)                             \
                                                                          \
	static inline void NAME##_swap_entries(void *arr[], int a, int b);    \
	static inline int  NAME##_swap_down(struct heap *h, int c);           \
	static inline int  NAME##_swap_up(struct heap *h, int c);             \
	static inline int  NAME##_heapify(struct heap *h, int c);             \
	static inline void NAME##_heap_adjust(struct heap *h, int c);         \
	static inline void *NAME##_heap_remove(struct heap *h, int c);        \
	static inline void *NAME##_heap_highest(struct heap *h);              \
	static inline int  NAME##_heap_add(struct heap *h, void *new);        \
	static inline int  NAME##_heap_verify(struct heap *h, int c);         \
    static inline void 	                                                  \
	NAME##_swap_entries(void *arr[], int a, int b)                        \
	{                                                            	      \
        void *t;                                                          \
                                                                          \
		t      = arr[a];                                                  \
        arr[a] = arr[b];                                                  \
        arr[b] = t;                                                       \
                                                                          \
        UPDATE_FN(arr[a], a);                                             \
        UPDATE_FN(arr[b], b);                                             \
    }                                                                     \
                                                                          \
    static inline int                                                     \
	NAME##_swap_down(struct heap *h, int c)                               \
    {                                                                     \
        int l;                                                            \
	    assert(c != 0);                                                   \
	    assert(c <= h->e);                                                \
	    l = h->e - 1;                                                     \
        while (c <= l / 2) {                                              \
            int n, left = 2 * c, right = 2 * c + 1;                       \
            if (right > l) {                                              \
                n = left;                                                 \
            } else if (CMP_FN(h->data[left], h->data[right])) {           \
                n = left;                                                 \
            } else {                                                      \
                n = right;                                                \
            }                                                             \
            assert(n < h->e);                                             \
            if (CMP_FN(h->data[c], h->data[n])) break;                    \
            NAME##_swap_entries(h->data, n, c);                           \
            c = n;                                                        \
        }                                                                 \
        return c;                                                         \
    }                                                                     \
                                                                          \
    static inline int                                                     \
	NAME##_swap_up(struct heap *h, int c)                                 \
	{                                                                     \
    	assert(c <= h->e);                                                \
		assert(c > 0);                                                    \
                                                                          \
	    while (c > 1) {                                                   \
            int p = c / 2;                                                \
			assert(p != 0);                                               \
            if (CMP_FN(h->data[p], h->data[c])) break;                    \
            NAME##_swap_entries(h->data, p, c);                           \
            c = p;                                                        \
		}                                                                 \
		assert(c != 0);                                                   \
        return c;                                                         \
    }                                                                     \
                                                                          \
    static inline int                                                     \
	NAME##_heapify(struct heap *h, int c)                                 \
	{                                                                     \
        c = NAME##_swap_up(h, c);                                         \
        return NAME##_swap_down(h, c);                                    \
    }                                                                     \
																		  \
	static inline int                                                     \
	NAME##_heap_verify(struct heap *h, int c)                             \
	{                                                                     \
		int left, right;                                                  \
																		  \
		left  = c * 2;													  \
		right = c * 2 + 1;                                                \
		if (left < h->e) {                                                \
			assert(((struct hentry *)h->data[left])->index == left);      \
			if (!CMP_FN(h->data[c], h->data[left]) || NAME##_heap_verify(h, left)) { \
				printc("Left data %d @ %d < %d @ %d\n", ((struct hentry *)h->data[c])->value, c, \
					((struct hentry *)h->data[left])->value, left);       \
				return 1;                                                 \
			}                                                             \
		}                                                                 \
		if (right < h->e) {                                               \
			assert(((struct hentry *)h->data[right])->index == right);    \
			if (!CMP_FN(h->data[c], h->data[right]) || NAME##_heap_verify(h, right)) { \
				printc("Right data %d @ %d < %d @ %d\n", ((struct hentry *)h->data[c])->value, c, \
					((struct hentry *)h->data[left])->value, left);       \
				return 1;                                                 \
			}                                                             \
		}                                                                 \
		return 0;                                                         \
	}                                                                     \
	                                                                      \
    static inline void                                                    \
	NAME##_heap_adjust(struct heap *h, int c)                             \
	{                                                                     \
		assert(c < h->e);                                                 \
		assert(c > 0);                                                    \
		debug("heap_adjust(%p,%d) %p@%d\n", h, h->e, h->data[c], c);      \
        NAME##_heapify(h, c);                                             \
    }                                                                     \
                                                                          \
    static inline void *                                                  \
	NAME##_heap_remove(struct heap *h, int c)                             \
	{                                                                     \
		void *removed;                                                    \
                                                                          \
		assert(c < h->e);                                                 \
		assert(c >= 1);                                                   \
		if (h->e == 1) return NULL;                                       \
                                                                          \
        removed = h->data[c];                                             \
		debug("heap_remove(%p,%d) %p@%d\n", h, h->e, h->data[c], c);      \
        h->e--;                                                           \
		UPDATE_FN(removed, 0);                                            \
        if (c == h->e) {                                                  \
			return removed;                                               \
		}                                                                 \
		h->data[c] = h->data[h->e];                                       \
		UPDATE_FN(h->data[c], c);                                         \
		NAME##_heap_adjust(h, c);                                         \
                                                                          \
		return removed;                                                   \
    }                                                                     \
                                                                          \
    void *                                                                \
	NAME##_heap_highest(struct heap *h)                                   \
	{                                                                     \
		void *r;                                                          \
		if (h->e == 1) return NULL;                                       \
		r = h->data[1];                                                   \
		debug("heap_highest(%p,%d) %p\n", h, h->e, r);                    \
                                                                          \
		h->e--;                                                           \
		h->data[1] = h->data[h->e];                                       \
		UPDATE_FN(h->data[1], 1);                                         \
		NAME##_swap_down(h, 1);                                           \
		UPDATE_FN(r, 0);                                                  \
	                                                                      \
		return r;                                                         \
    }                                                                     \
                                                                          \
    int                                                                   \
	NAME##_heap_add(struct heap *h, void *new)                            \
	{                                                                     \
		int c;                                                            \
		                                                                  \
		if (h->e >= h->max_sz) return -1;                                 \
		debug("heap_add(%p,%d) %p\n", h, h->e, new);                      \
                                                                          \
		c = h->e;                                                         \
        h->data[c] = new;                                                 \
        UPDATE_FN(new, c);                                                \
		h->e++;                                                           \
		NAME##_heapify(h, c);                                             \
		                                                                  \
        return 0;                                                         \
    }                                                                     

void *                                                  
heap_peek(struct heap *h)                                      
{                                                                     
	if (h->e == 1) return NULL;                                         
	return h->data[1];                                       
}  
															
void                                                    
heap_destroy(struct heap *h)                            
{                                                                      
	assert(h && h->data);                                             
																		
#ifdef LINUX                                                              
	free(h);                                                          
#endif                                                                    
}                                                                     
																		
struct heap *                                    
heap_alloc(int max_sz)                                         
{                                                                     
	struct heap *h = NULL;                                     
																		
#ifdef LINUX                                                              
	h = malloc(sizeof(struct heap) + (max_sz * sizeof(void *)) + 1); 
	if (NULL == h) return NULL;                                       
#endif                                                                           
	
	heap_init(h, max_sz);                                                                                                     
	return h;                                                         
}                                                                     
																		
void                                                    
heap_init(struct heap *h, int max_sz)                   
{                                                                     
	assert(h);                                                        
	h->e = 1;                                                         
	h->max_sz = max_sz + 1;                                           
	h->data   = (void *)&h[1];                                           
}                                                                     
																		
int                                                     
heap_size(struct heap *h)                               
{                                                                     
	return h->e - 1;                                                  
}                                                                     
																		
int                                                     
heap_empty(struct heap *h)                              
{                                                                     
	return heap_size(h) == 0;                                  
}                                                                     


#endif /* HEAP_H */