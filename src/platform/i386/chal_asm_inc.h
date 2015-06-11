/* Segment selectors for the GDT */
#define SEL_NULL	0x00
#define SEL_KDSEG       0x10    /* Kernel data selector. */
#define SEL_KCSEG       0x08    /* Kernel code selector. */
#define SEL_UCSEG       0x1B    /* User code selector. */
#define SEL_UDSEG       0x23    /* User data selector. */
#define SEL_TSS         0x28    /* Task-state segment. */
#define SEL_CNT         6       /* Number of segments. */

#define STK_INFO_SZ     24	/* sizeof(struct cos_cpu_local_info) */
