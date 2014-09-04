#ifndef _COS_IRQ_VECTORS_H
#define _COS_IRQ_VECTORS_H

/* The following vector entry should be free in Linux. Check
 * irq_vectors.h for detail layout in Linux. */

/* We should verify the IRQs are not used by Linux */
#define COS_IPI_VECTOR          0xe0
#define COS_REG_SAVE_VECTOR     0xe8

#endif /* _COS_IRQ_VECTORS_H */
