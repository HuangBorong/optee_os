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
#include <drivers/aplic.h>
#include <dt-bindings/interrupt-controller/irq.h>

#define APLIC_MAX_IDC			BIT(14) // 16384
#define APLIC_MAX_SOURCE		1024

#define APLIC_DOMAINCFG			0x0000
#define APLIC_DOMAINCFG_RDONLY		0x80000000
#define APLIC_DOMAINCFG_IE		BIT(8)  // Interrupt Enable
#define APLIC_DOMAINCFG_DM		BIT(2)  // Delivery Mode
#define APLIC_DOMAINCFG_BE		BIT(0)  // Big-Endian

#define APLIC_SOURCECFG_BASE		0x0004  //courcecfg[1]
#define APLIC_SOURCECFG_D		BIT(10)     // Delegate
#define APLIC_SOURCECFG_CHILDIDX_MASK	0x000003FF  // bits 9:0
#define APLIC_SOURCECFG_SM_MASK	0x00000007  // bits 2:0
#define APLIC_SOURCECFG_SM_INACTIVE	    0x0
#define APLIC_SOURCECFG_SM_DETACHED	    0x1
#define APLIC_SOURCECFG_SM_EDGE_RISE	0x4
#define APLIC_SOURCECFG_SM_EDGE_FALL	0x5
#define APLIC_SOURCECFG_SM_LEVEL_HIGH	0x6
#define APLIC_SOURCECFG_SM_LEVEL_LOW	0x7

#define APLIC_MMSIADDRCFG		0x1BC0
#define APLIC_MMSIADDRCFGH		0x1BC4
#define APLIC_SMSIADDRCFG		0x1BC8
#define APLIC_SMSIADDRCFGH		0x1BCC

/*#ifdef CONFIG_RISCV_M_MODE
#define APLIC_xMSICFGADDR		APLIC_MMSICFGADDR
#define APLIC_xMSICFGADDRH		APLIC_MMSICFGADDRH
#else
#define APLIC_xMSICFGADDR		APLIC_SMSICFGADDR
#define APLIC_xMSICFGADDRH		APLIC_SMSICFGADDRH
#endif

#define APLIC_xMSICFGADDRH_L		BIT(31)
#define APLIC_xMSICFGADDRH_HHXS_MASK	0x1f
#define APLIC_xMSICFGADDRH_HHXS_SHIFT	24
#define APLIC_xMSICFGADDRH_HHXS		(APLIC_xMSICFGADDRH_HHXS_MASK << \
					 APLIC_xMSICFGADDRH_HHXS_SHIFT)
#define APLIC_xMSICFGADDRH_LHXS_MASK	0x7
#define APLIC_xMSICFGADDRH_LHXS_SHIFT	20
#define APLIC_xMSICFGADDRH_LHXS		(APLIC_xMSICFGADDRH_LHXS_MASK << \
					 APLIC_xMSICFGADDRH_LHXS_SHIFT)
#define APLIC_xMSICFGADDRH_HHXW_MASK	0x7
#define APLIC_xMSICFGADDRH_HHXW_SHIFT	16
#define APLIC_xMSICFGADDRH_HHXW		(APLIC_xMSICFGADDRH_HHXW_MASK << \
					 APLIC_xMSICFGADDRH_HHXW_SHIFT)
#define APLIC_xMSICFGADDRH_LHXW_MASK	0xf
#define APLIC_xMSICFGADDRH_LHXW_SHIFT	12
#define APLIC_xMSICFGADDRH_LHXW		(APLIC_xMSICFGADDRH_LHXW_MASK << \
					 APLIC_xMSICFGADDRH_LHXW_SHIFT)
#define APLIC_xMSICFGADDRH_BAPPN_MASK	0xfff
#define APLIC_xMSICFGADDRH_BAPPN_SHIFT	0
#define APLIC_xMSICFGADDRH_BAPPN	(APLIC_xMSICFGADDRH_BAPPN_MASK << \
					 APLIC_xMSICFGADDRH_BAPPN_SHIFT)

#define APLIC_xMSICFGADDR_PPN_SHIFT	12

#define APLIC_xMSICFGADDR_PPN_HART(__lhxs) \
	(BIT(__lhxs) - 1)

#define APLIC_xMSICFGADDR_PPN_LHX_MASK(__lhxw) \
	(BIT(__lhxw) - 1)
#define APLIC_xMSICFGADDR_PPN_LHX_SHIFT(__lhxs) \
	((__lhxs))
#define APLIC_xMSICFGADDR_PPN_LHX(__lhxw, __lhxs) \
	(APLIC_xMSICFGADDR_PPN_LHX_MASK(__lhxw) << \
	 APLIC_xMSICFGADDR_PPN_LHX_SHIFT(__lhxs))

#define APLIC_xMSICFGADDR_PPN_HHX_MASK(__hhxw) \
	(BIT(__hhxw) - 1)
#define APLIC_xMSICFGADDR_PPN_HHX_SHIFT(__hhxs) \
	((__hhxs) + APLIC_xMSICFGADDR_PPN_SHIFT)
#define APLIC_xMSICFGADDR_PPN_HHX(__hhxw, __hhxs) \
	(APLIC_xMSICFGADDR_PPN_HHX_MASK(__hhxw) << \
	 APLIC_xMSICFGADDR_PPN_HHX_SHIFT(__hhxs))*/

#define APLIC_IRQBITS_PER_REG		32

#define APLIC_SETIP_BASE		0x1C00  // setip[0]
#define APLIC_SETIPNUM			0x1CDC

#define APLIC_IN_CLRIP_BASE		0x1D00  // in_clrip[0]
#define APLIC_CLRIPNUM			0x1DDC

#define APLIC_SETIE_BASE		0x1E00  // setie[0]
#define APLIC_SETIENUM			0x1EDC

#define APLIC_CLRIE_BASE		0x1F00  // clrie[0]
#define APLIC_CLRIENUM			0x1FDC

#define APLIC_SETIPNUM_LE		0x2000
#define APLIC_SETIPNUM_BE		0x2004

#define APLIC_GENMSI			0x3000

#define APLIC_TARGET_BASE		0x3004  // target[1]
#define APLIC_TARGET_HART_IDX_SHIFT	18
#define APLIC_TARGET_HART_IDX_MASK	0x3FFF
#define APLIC_TARGET_HART_IDX		(APLIC_TARGET_HART_IDX_MASK << \
                     APLIC_TARGET_HART_IDX_SHIFT)     // bits 31:18
#define APLIC_TARGET_GUEST_IDX_SHIFT	12
#define APLIC_TARGET_GUEST_IDX_MASK	0x3F
#define APLIC_TARGET_GUEST_IDX		(APLIC_TARGET_GUEST_IDX_MASK << \
					 APLIC_TARGET_GUEST_IDX_SHIFT)    // bits 17:12
#define APLIC_TARGET_EIID_SHIFT		0
#define APLIC_TARGET_EIID_MASK		0x7FF
#define APLIC_TARGET_EIID		(APLIC_TARGET_EIID_MASK << \
					 APLIC_TARGET_EIID_SHIFT)         // bits 10:0
#define APLIC_TARGET_IPRIO_SHIFT	0
#define APLIC_TARGET_IPRIO_MASK		0xFF
#define APLIC_TARGET_IPRIO		(APLIC_TARGET_IPRIO_MASK << \
                     APLIC_TARGET_IPRIO_SHIFT)        // bits 7:0

#define APLIC_IDC_BASE			0x4000
#define APLIC_IDC_SIZE			32

#define APLIC_IDC_IDELIVERY		0x00

#define APLIC_IDC_IFORCE		0x04

#define APLIC_IDC_ITHRESHOLD		0x08

#define APLIC_IDC_TOPI			0x18
#define APLIC_IDC_TOPI_ID_SHIFT		16
#define APLIC_IDC_TOPI_ID_MASK		0x3FF
#define APLIC_IDC_TOPI_ID		(APLIC_IDC_TOPI_ID_MASK << \
					 APLIC_IDC_TOPI_ID_SHIFT)       // bits 25:16
#define APLIC_IDC_TOPI_PRIO_SHIFT	0
#define APLIC_IDC_TOPI_PRIO_MASK	0xff
#define APLIC_IDC_TOPI_PRIO		(APLIC_IDC_TOPI_PRIO_MASK << \
					 APLIC_IDC_TOPI_PRIO_SHIFT)     // bits 7:0

#define APLIC_IDC_CLAIMI		0x1C

#define APLIC_DEFAULT_PRIORITY		1
#define APLIC_DISABLE_IDELIVERY		0
#define APLIC_ENABLE_IDELIVERY		1
#define APLIC_DISABLE_ITHRESHOLD	1
#define APLIC_ENABLE_ITHRESHOLD		0

struct aplic_data {
	vaddr_t 		aplic_base;
	uint32_t 		num_source;
	uint32_t 		num_idc;
	struct itr_chip chip;
};

static struct aplic_data aplic_data __nex_bss;

register_phys_mem_pgdir(MEM_AREA_IO_SEC, APLIC_BASE, APLIC_SIZE);

static vaddr_t aplic_get_idc_base(void)
{	
	struct aplic_data *aplic = &aplic_data;
	size_t hartid = get_core_pos();

	return aplic->aplic_base + APLIC_IDC_BASE + hartid * APLIC_IDC_SIZE;
}

static void aplic_enable_interrupt(struct aplic_data* aplic, uint32_t source)
{
	io_write32(aplic->aplic_base + APLIC_SETIENUM, source);
}

static void aplic_disable_interrupt(struct aplic_data* aplic, uint32_t source)
{
	io_write32(aplic->aplic_base + APLIC_CLRIENUM, source);
}

static void aplic_set_pending(struct aplic_data* aplic, uint32_t source)
{
	io_write32(aplic->aplic_base + APLIC_SETIPNUM, source);
}

static void aplic_clear_pending(struct aplic_data* aplic, uint32_t source)
{
	io_write32(aplic->aplic_base + APLIC_CLRIPNUM, source);
}

static TEE_Result aplic_set_source_mode(struct aplic_data* aplic, size_t it, uint32_t type)
{
	vaddr_t sourcecfg;
	uint32_t val;

	switch (type)
	{
	case IRQ_TYPE_NONE:
		val = APLIC_SOURCECFG_SM_INACTIVE;
		break;
	case IRQ_TYPE_EDGE_RISING:
		val = APLIC_SOURCECFG_SM_EDGE_RISE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		val = APLIC_SOURCECFG_SM_EDGE_FALL;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		val = APLIC_SOURCECFG_SM_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		val = APLIC_SOURCECFG_SM_LEVEL_LOW;
		break;	
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	sourcecfg = aplic->aplic_base + APLIC_SOURCECFG_BASE + (it - 1) * sizeof(uint32_t);
	io_write32(sourcecfg, val);

	return TEE_SUCCESS;
}

static void aplic_set_target(struct aplic_data* aplic, uint32_t source, uint32_t hart_idx, uint32_t iprio)
{
	vaddr_t target;
	uint32_t val = 0;

    val = (hart_idx & APLIC_TARGET_HART_IDX_MASK) << APLIC_TARGET_GUEST_IDX_SHIFT;
    val |= (iprio & APLIC_TARGET_IPRIO_MASK) << APLIC_TARGET_IPRIO_SHIFT;

	target = aplic->aplic_base + APLIC_TARGET_BASE + (source - 1) * sizeof(uint32_t);
	io_write32(target, val);
}

static void aplic_op_add(struct itr_chip *chip, size_t it, uint32_t type, uint32_t prio)
{
    struct aplic_data* aplic = container_of(chip, struct aplic_data, chip);
    size_t hartid = get_core_pos();

    if (!it || it > aplic->num_source)
        panic();
    
    aplic_disable_interrupt(aplic, it);
	if (aplic_set_source_mode(aplic, it, type))
		panic();
    aplic_set_target(aplic, it, hartid, prio);
}

static void aplic_op_enable(struct itr_chip *chip, size_t it)
{
	struct aplic_data *aplic = container_of(chip, struct aplic_data, chip);

	if (!it || it > aplic->num_source)
		panic();

	aplic_enable_interrupt(aplic, it);
}

static void aplic_op_disable(struct itr_chip *chip, size_t it)
{
	struct aplic_data *aplic = container_of(chip, struct aplic_data, chip);

	if (!it || it > aplic->num_source)
		panic();

	aplic_disable_interrupt(aplic, it);
}

static void aplic_op_raise_pi(struct itr_chip *chip, size_t it)
{
    struct aplic_data* aplic = container_of(chip, struct aplic_data, chip);

    if (!it || it > aplic->num_source)
        panic();

    aplic_set_pending(aplic, it);
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

static void aplic_init_base_addr(struct aplic_data *aplic, paddr_t aplic_base_pa)
{
	vaddr_t aplic_base = 0;

	assert(cpu_mmu_enabled());

	aplic_base = core_mmu_get_va(aplic_base_pa, MEM_AREA_IO_SEC,
				    APLIC_SIZE);
	if (!aplic_base)
		panic();

	aplic->aplic_base = aplic_base;
	aplic->num_source = APLIC_NUM_SOURCE;
	aplic->num_idc = APLIC_NUM_IDC;
	aplic->chip.ops = &aplic_ops;
}

void aplic_init(paddr_t aplic_base_pa)
{
	struct aplic_data *aplic = &aplic_data;
	uint32_t id = 0;

	aplic_init_base_addr(aplic, aplic_base_pa);

	io_write32(aplic->aplic_base + APLIC_DOMAINCFG, APLIC_DOMAINCFG_IE);
	aplic_hart_init();

	interrupt_main_init(&aplic_data.chip);
}

void aplic_hart_init(void)
{	
	struct aplic_data *aplic = &aplic_data;
	vaddr_t idc_base = aplic_get_idc_base();

	io_write32(idc_base + APLIC_IDC_IDELIVERY, APLIC_ENABLE_IDELIVERY);
	io_write32(idc_base + APLIC_IDC_ITHRESHOLD, APLIC_ENABLE_ITHRESHOLD);
}

void aplic_it_handle(void)
{
	struct aplic_data *aplic = &aplic_data;
	uint32_t id = 0;

	id = io_read32(aplic->aplic_base + APLIC_IDC_CLAIMI);
	id >>= APLIC_IDC_TOPI_ID_SHIFT;

	if (id <= aplic->num_source)
		interrupt_call_handlers(&aplic->chip, id);
	else
		DMSG("ignoring interrupt %" PRIu32, id);
}
