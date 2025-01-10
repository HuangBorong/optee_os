#ifndef __DRIVERS_APLIC_MSI_H
#define __DRIVERS_APLIC_MSI_H

#include <types_ext.h>
#include <kernel/interrupt.h>

void aplic_msi_init(paddr_t aplic_base_pa);
void aplic_msi_hart_init(void);
void aplic_msi_it_handle(uint32_t it);
void aplic_msi_dump_state(void);
struct itr_chip *aplic_msi_get_chip(void);

#endif /*__DRIVERS_APLIC_MSI_H*/