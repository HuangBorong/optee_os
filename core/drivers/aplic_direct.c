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
#include <drivers/aplic_direct.h>

#define APLIC_MAX_IDC			           BIT(14)  // 16384
#define APLIC_IDC_BASE			            0x4000
#define APLIC_IDC_SIZE			                32

#define APLIC_IDC_IDELIVERY                   0x00  // idelivery
#define APLIC_IDC_IFORCE		              0x04  // iforce
#define APLIC_IDC_ITHRESHOLD	              0x08  // ithreshold

#define APLIC_IDC_TOPI			              0x18  // topi
#define APLIC_IDC_TOPI_ID_SHIFT		            16
#define APLIC_IDC_TOPI_ID_MASK               0x3FF
#define APLIC_IDC_TOPI_ID		    (APLIC_IDC_TOPI_ID_MASK << \
					    APLIC_IDC_TOPI_ID_SHIFT)    // bits 25:16
#define APLIC_IDC_TOPI_PRIO_SHIFT	             0
#define APLIC_IDC_TOPI_PRIO_MASK	          0xFF
#define APLIC_IDC_TOPI_PRIO		    (APLIC_IDC_TOPI_PRIO_MASK << \
					    APLIC_IDC_TOPI_PRIO_SHIFT)  // bits 7:0

#define APLIC_IDC_CLAIMI		              0x1C  // claimi

#define APLIC_DEFAULT_PRIORITY		1

#define APLIC_DISABLE_IDELIVERY		0
#define APLIC_ENABLE_IDELIVERY		1

#define APLIC_DISABLE_ITHRESHOLD	1
#define APLIC_ENABLE_ITHRESHOLD		0

struct aplic_direct
{
    struct aplic_priv   priv;
    uint32_t 		 num_idc;
	struct itr_chip     chip;
};

static struct aplic_direct aplic_direct __nex_bss;

register_phys_mem_pgdir(MEM_AREA_IO_SEC, APLIC_BASE, APLIC_SIZE);

static vaddr_t aplic_get_idc_base(void)
{	
	struct aplic_direct *aplic = &aplic_direct;
	size_t hartid = get_core_pos();

	return aplic->priv.aplic_base + APLIC_IDC_BASE + hartid * APLIC_IDC_SIZE;
}

static void aplic_set_target(struct aplic_direct* aplic, uint32_t source, uint32_t hart_idx, uint32_t iprio)
{
	vaddr_t target;
	uint32_t val = 0;

    val = (hart_idx & APLIC_TARGET_HART_IDX_MASK) << APLIC_TARGET_HART_IDX_SHIFT;
    val |= (iprio & APLIC_TARGET_IPRIO_MASK) << APLIC_TARGET_IPRIO_SHIFT;

	target = aplic->priv.aplic_base + APLIC_TARGET_BASE + (source - 1) * sizeof(uint32_t);
	io_write32(target, val);
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
    struct aplic_direct* aplic = container_of(chip, struct aplic_direct, chip);
    struct aplic_priv* priv = &aplic->priv;
    size_t hartid = get_core_pos();

    if (!it || it > priv->num_source)
        panic();
    
    aplic_disable_interrupt(priv, it);
	if (aplic_set_source_mode(priv, it, type))
		panic();
    aplic_set_target(aplic, it, hartid, prio);
}

static void aplic_op_enable(struct itr_chip *chip, size_t it)
{
	struct aplic_direct* aplic = container_of(chip, struct aplic_direct, chip);
    struct aplic_priv* priv = &aplic->priv;

	if (!it || it > priv->num_source)
        panic();

	aplic_enable_interrupt(priv, it);
}

static void aplic_op_disable(struct itr_chip *chip, size_t it)
{
	struct aplic_direct* aplic = container_of(chip, struct aplic_direct, chip);
    struct aplic_priv* priv = &aplic->priv;

	if (!it || it > priv->num_source)
        panic();

	aplic_disable_interrupt(priv, it);
}

static void aplic_op_raise_pi(struct itr_chip *chip, size_t it)
{
	struct aplic_direct* aplic = container_of(chip, struct aplic_direct, chip);
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

void aplic_direct_init(paddr_t aplic_base_pa)
{
	struct aplic_direct *aplic = &aplic_direct;
    struct aplic_priv* priv = &aplic->priv;
	uint32_t id = 0;

	aplic_init_base_addr(aplic, aplic_base_pa);

	io_write32(priv->aplic_base + APLIC_DOMAINCFG, APLIC_DOMAINCFG_IE);
	aplic_hart_init();

	interrupt_main_init(&aplic_direct.chip);
}

void aplic_direct_hart_init(void)
{
	vaddr_t idc_base = aplic_get_idc_base();

	io_write32(idc_base + APLIC_IDC_IDELIVERY, APLIC_ENABLE_IDELIVERY);
	io_write32(idc_base + APLIC_IDC_ITHRESHOLD, APLIC_ENABLE_ITHRESHOLD);
}

void aplic_direct_it_handle(void)
{
	struct aplic_direct *aplic = &aplic_direct;
    struct aplic_priv* priv = &aplic->priv;
	uint32_t id = 0;

	id = io_read32(priv->aplic_base + APLIC_IDC_CLAIMI);
	id >>= APLIC_IDC_TOPI_ID_SHIFT;

	if (id <= priv->num_source)
		interrupt_call_handlers(&aplic->chip, id);
	else
		DMSG("ignoring interrupt %" PRIu32, id);
}

void aplic_direct_dump_state(void)
{
}