#ifndef COS_DPDK_ADAPTER_H
#define COS_DPDK_ADAPTER_H

int cos_printc(const char *fmt,va_list ap); 
int cos_printf(const char *fmt,...);

int cos_bus_scan(void);
#endif /* COS_DPDK_ADAPTER_H */