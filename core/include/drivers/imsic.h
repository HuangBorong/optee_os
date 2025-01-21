#ifndef __DRIVERS_IMSIC_H
#define __DRIVERS_IMSIC_H

#include <types_ext.h>
#include <tee_api_defines.h>
#include <kernel/interrupt.h>

#define IMSIC_MAX_REGS			16

struct imsic_data {
	vaddr_t imsic_base;
	uint32_t size;
	bool targets_mmode;
	uint32_t num_ids;
	uint32_t guest_index_bits;
	uint32_t hart_index_bits;
	uint32_t group_index_bits;
	uint32_t group_index_shift;
	struct itr_chip chip;
};

void imsic_init(paddr_t imsic_base_pa);
void imsic_hart_init(void);
void imsic_it_handle(void);
void imsic_dump_state(void);
#ifdef CFG_RISCV_IMSIC
TEE_Result fdt_parse_imsic_node(const void *fdt, int nodeoff, struct imsic_data *imsic);
#else
static inline TEE_Result fdt_parse_imsic_node(const void *fdt __unused, int nodeoff __unused, struct imsic_data *imsic __unused)
{
	return TEE_ERROR_ITEM_NOT_FOUND;
}
#endif /* CFG_RISCV_IMSIC */

#endif /* __DRIVERS_IMSIC_H */