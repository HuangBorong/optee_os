// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2022-2023 NXP
 */

#include <console.h>
#include <drivers/aplic.h>
#include <drivers/imsic.h>
#include <drivers/ns16550.h>
#include <drivers/plic.h>
#include <kernel/boot.h>
#include <kernel/tee_common_otp.h>
#include <platform_config.h>

#define SECURE_TIMER_BASE 0x41000000
#define SECURE_TIMER_SIZE 0x1000
#define SECURE_TIMER_IRQ   20

#define SEC_TIMER_CFG_OFFSET	0x00
#define SEC_TIMER_CMP_OFFSET	0x04

volatile static vaddr_t timer_base;
register_phys_mem(MEM_AREA_IO_SEC, SECURE_TIMER_BASE, SECURE_TIMER_SIZE);

#ifdef CFG_16550_UART
static struct ns16550_data console_data __nex_bss;
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, UART0_BASE, CORE_MMU_PGDIR_SIZE);
#endif

register_ddr(DRAM_BASE, DRAM_SIZE);

#if defined(CFG_RISCV_APLIC) || defined(CFG_RISCV_APLIC_MSI)
register_phys_mem_pgdir(MEM_AREA_IO_SEC, APLIC_BASE,
			APLIC_SIZE);
#endif
#if defined(CFG_RISCV_APLIC_MSI) && defined(CFG_RISCV_IMSIC)
register_phys_mem_pgdir(MEM_AREA_IO_SEC, IMSIC_BASE,
			IMSIC_SIZE);
#endif

#ifdef CFG_RISCV_PLIC
void boot_primary_init_intc(void)
{
	plic_init(PLIC_BASE);
}

void boot_secondary_init_intc(void)
{
	plic_hart_init();
}
#endif /* CFG_RISCV_PLIC */

#ifdef CFG_RISCV_APLIC
void boot_primary_init_intc(void)
{
	aplic_init(APLIC_BASE);
}

void boot_secondary_init_intc(void)
{
	aplic_init_per_hart();
}
#endif /* CFG_RISCV_APLIC */

#if defined(CFG_RISCV_APLIC_MSI) && defined(CFG_RISCV_IMSIC)
void boot_primary_init_intc(void)
{
	aplic_init(APLIC_BASE);
	imsic_init(IMSIC_BASE);
}

void boot_secondary_init_intc(void)
{
	aplic_init_per_hart();
	imsic_init_per_hart();
}
#endif

#ifdef CFG_16550_UART
void plat_console_init(void)
{
	ns16550_init(&console_data, UART0_BASE, IO_WIDTH_U8, 0);
	register_serial_console(&console_data.chip);
}
#endif

void interrupt_main_handler(void)
{
	IMSG("we are in interrupt main handler");
	if (IS_ENABLED(CFG_RISCV_PLIC))
		plic_it_handle();
	else if (IS_ENABLED(CFG_RISCV_APLIC))
		aplic_it_handle();
	else if (IS_ENABLED(CFG_RISCV_APLIC_MSI) &&
		 IS_ENABLED(CFG_RISCV_IMSIC))
		imsic_it_handle();
}

static void read_console(void)
{
	struct serial_chip *cons = &console_data.chip;

	if (!cons->ops->getchar || !cons->ops->have_rx_data)
		return;

	while (cons->ops->have_rx_data(cons)) {
		int ch __maybe_unused = cons->ops->getchar(cons);

		IMSG("got 0x%x", ch);
	}
}

static enum itr_return console_itr_cb(struct itr_handler *h __maybe_unused)
{
	read_console();
	return ITRR_HANDLED;
}

static struct itr_handler console_itr = {
	.it = UART0_IRQ,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = console_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);

static TEE_Result init_console_itr(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	console_itr.chip = interrupt_get_main_chip();
	res = interrupt_add_configure_handler(&console_itr, IRQ_TYPE_LEVEL_HIGH,
					      1);
	if (res)
		return res;

	interrupt_enable(console_itr.chip, console_itr.it);

	return TEE_SUCCESS;
}
driver_init(init_console_itr);

static enum itr_return timer_itr_cb(struct itr_handler *h __unused)
{
	io_write32(timer_base + SEC_TIMER_CFG_OFFSET, 0x00000000);
	io_write32(timer_base + SEC_TIMER_CMP_OFFSET, 0x00000000);

	IMSG("Timer Interrupt handled");

	return ITRR_HANDLED;
}

static struct itr_handler timer_itr = {
	.it = SECURE_TIMER_IRQ,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = timer_itr_cb,
};

static TEE_Result init_timer_itr(void)
{	
	TEE_Result res = TEE_ERROR_GENERIC;

	timer_itr.chip = interrupt_get_main_chip();
	res = interrupt_add_configure_handler(&timer_itr, IRQ_TYPE_LEVEL_HIGH,
		1);
	if (res)
		return res;

	interrupt_enable(timer_itr.chip, timer_itr.it);

	timer_base = core_mmu_get_va(SECURE_TIMER_BASE, MEM_AREA_IO_SEC, SECURE_TIMER_SIZE);

	return TEE_SUCCESS;
}
driver_init(init_timer_itr);

void main_loop(void)
{	
	int ch;
	
	aplic_dump_state();

	io_write32(timer_base + SEC_TIMER_CMP_OFFSET, 8000);
	io_write32(timer_base + SEC_TIMER_CFG_OFFSET, 0xFFFF);

	while (1)
		;
}