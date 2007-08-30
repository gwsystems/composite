/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#define PAGE_SIZE (1<<12)
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PGD_RANGE (1<<22)
#define PGD_MASK  (~(PGD_RANGE-1))

#define round_to_page(x) ((x)&PAGE_MASK)
#define round_up_to_page(x) (((x)+PAGE_SIZE-1)&PAGE_MASK)
#define round_to_pgd_page(x) ((x)&PGD_MASK)
#define round_up_to_pgd_page(x) (((x)+PGD_RANGE-1)&PGD_MASK)

#define CACHE_LINE (32)
#define CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE)))
#define HALF_CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE/2)))

#define INV_STK_START (1<<30)  // 1 gig
#define INV_STK_SIZE PAGE_SIZE
/* size of virtual address spanned by one pgd entry */
#define SERVICE_SIZE PGD_RANGE
