#include <io.h>
#include <tee_api_types.h>
#include <drivers/aplic_priv.h>
#include <dt-bindings/interrupt-controller/irq.h>

TEE_Result aplic_set_source_mode(struct aplic_priv* priv, size_t it, uint32_t type)
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

	sourcecfg = priv->aplic_base + APLIC_SOURCECFG_BASE + (it - 1) * sizeof(uint32_t);
	io_write32(sourcecfg, val);

	return TEE_SUCCESS;
}
