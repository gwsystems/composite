#ifndef PCI_H
#define PCI_H

#define PCI_BUS_MAX        256
#define PCI_DEVICE_MAX     32
#define PCI_FUNC_MAX       8
#define PCI_DEVICE_NUM     (PCI_BUS_MAX * PCI_DEVICE_MAX * PCI_FUNC_MAX)
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
#define PCI_BAR_START      0x04
#define PCI_COMMON_DATA_SZ 0x10
#define PCI_BAR_NUM        6
#define PCI_BITMASK_32     0xFFFFFFFF

enum {
	PCI_TYPE_DEVICE = 0,
	PCI_TYPE_PCI_TO_PCI_BRIDGE = 1,
	PCI_TYPE_PCI_TO_CARDBUS_BRIDGE = 2,
	PCI_TYPE_INVALID
};

enum {
	PCI_BAR_MEM = 0,
	PCI_BAR_IO = 1
};

enum {
	PCI_MEM_BAR_32 = 0,
	PCI_MEM_BAR_64 = 2
};

/* /1* pci CONFIG_ADDRESS register format *1/ */
/* struct pci_addr_layout { */
/* 	uint32 regNum:8; */
/* 	uint32 funcNum:3; */
/* 	uint32 devNum:5; */
/* 	uint32 busNum:8; */
/* 	uint32 reserved:7; */
/* 	uint32 enable:1; */
/* }; */
#define PCI_ADDR(bus, dev, func, reg) ((1 << 31) | ((bus) << 16) | ((dev) << 11) | ((func) << 8) | (reg))
#define PCI_VENDOR_ID(v)   ((v) & 0xFFFF)
#define PCI_DEVICE_ID(v)   (((v) >> 16) & 0xFFFF)
#define PCI_CLASS_ID(v)    (((v) >> 24) & 0xFF)
#define PCI_SUBCLASS_ID(v) (((v) >> 16) & 0xFF)
#define PCI_PROG_IF(v)     (((v) >> 8) & 0xFF)
#define PCI_HEADER(v)      (((v) >> 16) & 0xFF)

#define PCI_SUBSYSTEM_ID(v) ((v) >> 16)
#define PCI_SUBSYSTEM_VENDOR_ID(v) ((v) & 0xFFFF) 

struct pci_bar {
	union {
		u32_t raw;
		struct ioBAR {
			u32_t setBit:1;
			u32_t reserved:1;
			u32_t ioPort:30;
		} __attribute__((packed)) io;
		struct memBAR {
			u32_t clrBit:1;
			u32_t memType:2;
			u32_t prefetch:1;
			u32_t baseAddr:28;
		} __attribute__((packed)) mem;
	};
	u32_t mask;
	int   accessibility;
	u8_t  bar_type;
	u64_t paddr;
	u64_t len;
} __attribute__((packed));

struct pci_dev {
	u32_t bus, dev, func;
	u16_t vendor;
	u16_t device;
	u8_t  classcode;
	u8_t  subclass;
	u8_t  progIF;
	union
	{
		/* data */
		u8_t pci_type : 7; /* indicate pci type */
		u8_t MF   : 1; /* indicate if this pci device has multiple functions */
		u8_t header;
	};

	u16_t subsystem_vendor_id; 
	u16_t subsystem_device_id; 

	struct pci_bar bar[PCI_BAR_NUM];
	u32_t data[PCI_COMMON_DATA_SZ]; /* keep raw values of PCI config space data */
	unsigned int index;
	void *drvdata;
} __attribute__((packed));

/**
 * scans through the pci bus and fills the provided array
 * @param devices array of `struct pci_dev`
 * @param sz the length of devices (not all space will necessarily be filled, this is just the total capacity)
 * @return 0 on success, -1 on failure
 */
int pci_scan(struct pci_dev *devices, int sz);

/**
 * iterates through the array and prints out device id, vendor id, and classcode for each device
 * @param devices array to iterate through
 * @param sz the length of devices
 */
void pci_dev_print(struct pci_dev *devices, int sz);

/** 
 * populate pci_dev pointer with device associated with the vendor/device id, if it exists
 * @param devices array to iterate through
 * @param sz the length of devices
 * @param dev the pointer to fill
 * @param dev_id the device id of the device we're looking for
 * @param vendor_id the vendor id of the device we're looking for
 * @return 0 on success, -1 if the device isn't found
 */
int pci_dev_get(struct pci_dev *devices, int sz, struct pci_dev *dev, u16_t dev_id, u16_t vendor_id);

/**
 * returns total number of pci devices on the bus
 */
int pci_dev_count(void);

u32_t pci_config_read(u32_t bus, u32_t dev, u32_t func, u32_t reg);
void pci_config_write(u32_t bus, u32_t dev, u32_t func, u32_t reg, u32_t v);
#endif /* PCI_H */