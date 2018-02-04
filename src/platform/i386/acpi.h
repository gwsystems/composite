#ifndef ACPI_H
#define ACPI_H

void *acpi_find_apic(void);
void *acpi_find_rsdt(void);
void *acpi_find_hpet(void);
void  acpi_set_rsdt_page(u32_t);

#endif /* ACPI_H */
