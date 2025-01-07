#ifndef __DRIVERS_APLIC_H
#define __DRIVERS_APLIC_H

#include <types_ext.h>

void aplic_init(paddr_t aplic_base_pa);
void aplic_hart_init(void);
void aplic_it_handle(void);
void aplic_dump_state(void);

#endif /*__DRIVERS_APLIC_H*/