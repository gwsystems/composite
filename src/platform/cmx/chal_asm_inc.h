#define STK_INFO_SZ     96	/* sizeof(struct cos_cpu_local_info) */
#define STK_INFO_OFF    (STK_INFO_SZ + 2048)	/* sizeof(struct cos_cpu_local_info) + make it larger to avoid flushing stack return address on CM7 sizeof(long) */
