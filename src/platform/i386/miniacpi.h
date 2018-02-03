#ifndef MINIACPI_H
#define MINIACPI_H

void *miniacpi_find_apic(void);
void *miniacpi_find_rsdt(void);
void *miniacpi_find_hpet(void);
void  miniacpi_set_rsdt_page(u32_t);

#endif /* MINIACPI_H */
