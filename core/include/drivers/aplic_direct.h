#ifndef __DRIVERS_APLIC_DIRECT_H
#define __DRIVERS_APLIC_DIRECT_H

#include <types_ext.h>

void aplic_direct_init(paddr_t aplic_base_pa);
void aplic_direct_hart_init(void);
void aplic_direct_it_handle(void);
void aplic_direct_dump_state(void);

#endif /*__DRIVERS_APLIC_DIRECT_H*/