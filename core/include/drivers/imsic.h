#ifndef __DRIVERS_IMSIC_H
#define __DRIVERS_IMSIC_H

#include <types_ext.h>

void imsic_init(paddr_t imsic_base_pa);
void imsic_hart_init(void);
void imsic_it_handle(void);
void imsic_dump_state(void);

#endif