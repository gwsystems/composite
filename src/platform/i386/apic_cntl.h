#ifndef APIC_CNTL_H
#define APIC_CNTL_H

#define APIC_DEFAULT_PHYS     0xFEE00000
#define APIC_HDR_LEN_OFF      0x04
#define APIC_CNTRLR_ADDR_OFF  0x24
#define APIC_CNTRLR_FLAGS_OFF 0x28
#define APIC_CNTR_ARR_OFF     0x2C

/* See 5.2.12 in the ACPI 5.0 Spec */
enum
{
	APIC_CNTL_LAPIC  = 0,
	APIC_CNTL_IOAPIC = 1,
	APIC_CNTL_ISO    = 2,
};

struct int_cntl_head {
	u8_t type;
	u8_t len;
} __attribute__((packed));

struct lapic_cntl {
	/* type == APIC_CNTL_LAPIC */
	struct int_cntl_head header;
	u8_t                 proc_id;
	u8_t                 apic_id;
	u32_t                flags; /* 0 = dead processor */
} __attribute__((packed));

struct ioapic_cntl {
	/* type == APIC_CNTL_IOAPIC */
	struct int_cntl_head header;
	u8_t                 ioapic_id;
	u8_t                 reserved;
	u32_t                ioapic_phys_addr;
	u32_t                glb_int_num_off; /* I/O APIC's interrupt base number offset  */
} __attribute__((packed));

struct intsrcovrride_cntl {
	/* type == APIC_CNTL_ISO */
	struct int_cntl_head header;
	u8_t                 bus;
	u8_t                 source;
	u32_t                glb_int_num_off;
	u16_t                flags;
} __attribute__((packed));

enum acpi_madt_iso_polarity {
	ACPI_MADT_ISO_POL_CONFORMS = 0,
	ACPI_MADT_ISO_POL_ACTHIGH,
	ACPI_MADT_ISO_POL_RESERVED,
	ACPI_MADT_ISO_POL_ACTLOW,
};

enum acpi_madt_iso_trigger {
	ACPI_MADT_ISO_TRIG_CONFORMS = 0,
	ACPI_MADT_ISO_TRIG_EDGE,
	ACPI_MADT_ISO_TRIG_RESERVED,
	ACPI_MADT_ISO_TRIG_LEVEL,
};

#endif /* APIC_CNTL_H */
