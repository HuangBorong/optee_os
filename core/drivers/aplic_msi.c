#include <io.h>
#include <util.h>
#include <trace.h>
#include <assert.h>
#include <config.h>
#include <types_ext.h>
#include <platform_config.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/interrupt.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <drivers/aplic_priv.h>
#include <drivers/aplic_msi.h>

#define APLIC_DEFAULT_EIID		2

struct aplic_msi
{
    struct aplic_priv   priv;
	struct itr_chip     chip;
};

static struct aplic_msi aplic_msi __nex_bss;

register_phys_mem_pgdir(MEM_AREA_IO_SEC, APLIC_BASE, APLIC_SIZE);

static void aplic_set_target(struct aplic_msi* aplic, uint32_t source, uint32_t hart_idx, uint32_t guest_idx, uint32_t eiid)
{
	vaddr_t target;
	uint32_t val = 0;

    val = (hart_idx & APLIC_TARGET_HART_IDX_MASK) << APLIC_TARGET_HART_IDX_SHIFT;
    val |= (guest_idx & APLIC_TARGET_GUEST_IDX_MASK) << APLIC_TARGET_GUEST_IDX_SHIFT;
	val |= (eiid & APLIC_TARGET_EIID_MASK) << APLIC_TARGET_EIID_SHIFT;

	target = aplic->priv.aplic_base + APLIC_TARGET_BASE + (source - 1) * sizeof(uint32_t);
	io_write32(target, val);
}

static uint32_t aplic_get_source_mode(struct aplic_msi* aplic, uint32_t source)
{
	struct aplic_priv* priv = &aplic->priv;
	uint32_t sm = 0;

	sm = io_read32(priv->aplic_base + APLIC_SOURCECFG_BASE + (source - 1) * sizeof(uint32_t));

	return sm & APLIC_SOURCECFG_SM_MASK;
}

static void aplic_msi_irq_retrigger_level(struct aplic_msi *aplic, uint32_t source)
{
	struct aplic_priv* priv = &aplic->priv;

	switch (aplic_get_source_mode(aplic, source)) {
	case APLIC_SOURCECFG_SM_LEVEL_HIGH:
	case APLIC_SOURCECFG_SM_LEVEL_LOW:
		/*
		 * The section "4.9.2 Special consideration for level-sensitive interrupt
		 * sources" of the RISC-V AIA specification says:
		 *
		 * A second option is for the interrupt service routine to write the
		 * APLIC’s source identity number for the interrupt to the domain’s
		 * setipnum register just before exiting. This will cause the interrupt’s
		 * pending bit to be set to one again if the source is still asserting
		 * an interrupt, but not if the source is not asserting an interrupt.
		 */
		io_write32(priv->aplic_base + APLIC_SETIPNUM, source);
		break;
	}
}

static void aplic_init_base_addr(struct aplic_direct *aplic, paddr_t aplic_base_pa)
{
    struct aplic_priv* priv = &aplic->priv;
	vaddr_t aplic_base = 0;

	assert(cpu_mmu_enabled());

	aplic_base = core_mmu_get_va(aplic_base_pa, MEM_AREA_IO_SEC,
				    APLIC_SIZE);
	if (!aplic_base)
		panic();

	priv->aplic_base = aplic_base;
	priv->num_source = APLIC_NUM_SOURCE;

	aplic->num_idc = APLIC_NUM_IDC;
	aplic->chip.ops = &aplic_ops;
}

static void aplic_op_add(struct itr_chip *chip, size_t it, uint32_t type, uint32_t prio)
{
    struct aplic_msi* aplic = container_of(chip, struct aplic_msi, chip);
    struct aplic_priv* priv = &aplic->priv;
    size_t hartid = get_core_pos();

    if (!it || it > priv->num_source)
        panic();
    
    aplic_disable_interrupt(priv, it);
	if (aplic_set_source_mode(priv, it, type))
		panic();
	/*
	 * Updating sourcecfg register for level-triggered interrupts
	 * requires interrupt retriggering when APLIC is in MSI mode.
	 */
	aplic_msi_irq_retrigger_level(aplic, it);
    aplic_set_target(aplic, it, hartid, 0, APLIC_DEFAULT_EIID);
}

static void aplic_op_enable(struct itr_chip *chip, size_t it)
{
	struct aplic_msi* aplic = container_of(chip, struct aplic_msi, chip);
    struct aplic_priv* priv = &aplic->priv;

	if (!it || it > priv->num_source)
        panic();

	aplic_enable_interrupt(priv, it);
}

static void aplic_op_disable(struct itr_chip *chip, size_t it)
{
	struct aplic_msi* aplic = container_of(chip, struct aplic_msi, chip);
    struct aplic_priv* priv = &aplic->priv;

	if (!it || it > priv->num_source)
        panic();

	aplic_disable_interrupt(priv, it);
}

static void aplic_op_raise_pi(struct itr_chip *chip, size_t it)
{
	struct aplic_msi* aplic = container_of(chip, struct aplic_msi, chip);
    struct aplic_priv* priv = &aplic->priv;

	if (!it || it > priv->num_source)
        panic();

	aplic_set_pending(priv, it);
}

static const struct itr_ops aplic_ops = {
	.add = aplic_op_add,
	.enable = aplic_op_enable,
	.disable = aplic_op_disable,
	.mask = aplic_op_disable,
	.unmask = aplic_op_enable,
	.raise_pi = aplic_op_raise_pi,
	.raise_sgi = NULL,
	.set_affinity = NULL
};

void aplic_msi_init(paddr_t aplic_base_pa)
{
	struct aplic_msi *aplic = &aplic_msi;
    struct aplic_priv* priv = &aplic->priv;
	uint32_t id = 0;

	aplic_init_base_addr(aplic, aplic_base_pa);

	io_write32(priv->aplic_base + APLIC_DOMAINCFG, APLIC_DOMAINCFG_IE | APLIC_DOMAINCFG_DM);
	aplic_hart_init();
}

void aplic_msi_hart_init(void)
{
}

void aplic_msi_it_handle(uint32_t it)
{
	struct aplic_msi *aplic = &aplic_msi;

	aplic_msi_irq_retrigger_level(aplic, it);
}

struct itr_chip *aplic_msi_get_chip(void)
{
    return &aplic_msi.chip ? &aplic_msi.chip : NULL;
}

