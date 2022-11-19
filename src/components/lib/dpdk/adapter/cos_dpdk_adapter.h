#ifndef COS_DPDK_ADAPTER_H
#define COS_DPDK_ADAPTER_H

#define COS_DPDK_NUM_CPU 1
#define COS_PCI_IO_SIZE 4

typedef unsigned long cos_paddr_t; /* physical address */
typedef unsigned long cos_vaddr_t; /* virtual address */

/* Declare DPDK data structures used within adapter to avoid compiler warnings */
struct rte_pci_device;


/*
 * Since some libs/drivers cannot be linked automatically within by the linker through libdpdk.a,
 * these two macros are used to force link these modules into the ourput program binaries, users
 * need to first *define* the module within a target module's .c file, and then *declare* that
 * module name in cos_dpdk_adapter.c
 */
#define COS_DPDK_DECLARE_NIC_MODULE(module_name) extern char cos_dpdk_##module_name##_nic_module;static const char* cos_dpdk_##module_name##_ptr __attribute__((used)) = &cos_dpdk_##module_name##_nic_module;
#define COS_DPDK_DEFINE_NIC_MODULE(module_name) char cos_dpdk_##module_name##_nic_module;

/* Adapters definitions */
int cos_printc(const char *fmt,va_list ap);
int cos_printf(const char *fmt,...);

int cos_bus_scan(void);
int cos_pci_scan(void);

int cos_pci_write_config(const struct rte_pci_device *device, const void *buf, size_t len, off_t offset);
int cos_pci_read_config(const struct rte_pci_device *device, void *buf, size_t len, off_t offset);

cos_vaddr_t cos_map_phys_to_virt(cos_paddr_t paddr, size_t size);
cos_paddr_t cos_map_virt_to_phys(cos_vaddr_t addr);

unsigned long cos_get_tsc_freq(void);
unsigned cos_curr_lcore(void);
#endif /* COS_DPDK_ADAPTER_H */