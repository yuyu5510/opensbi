/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 * Copyright (c) 2025 Archlab, Science Tokyo
 * 
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_system.h>
#include <libfdt.h>

/*
 * Include these files as needed.
 * See objects.mk PLATFORM_xxx configuration parameters.
 */
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/rvcpu-uart.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define PLATFORM_PLIC_ADDR           0xc000000
#define PLATFORM_PLIC_SIZE           (0x200000 + (PLATFORM_HART_COUNT * 0x1000))
#define PLATFORM_PLIC_NUM_SOURCES    1
#define PLATFORM_HART_COUNT          1
#define PLATFORM_CLINT_ADDR          0x2000000
#define PLATFORM_ACLINT_MTIMER_FREQ  10000000
#define PLATFORM_ACLINT_MSWI_ADDR    (PLATFORM_CLINT_ADDR + CLINT_MSWI_OFFSET)
#define PLATFORM_ACLINT_MTIMER_ADDR  (PLATFORM_CLINT_ADDR + CLINT_MTIMER_OFFSET)
#define PLATFORM_UART_ADDR           0x10000000
#define PLATFORM_RST_CTRL_ADDR       0x10000100
#define PLATFORM_MMC_FLUSH_ADDR      0xa0000018
#define PLATFORM_MMC_FLUSH_DONE_ADDR 0xa000001c
#define PLATFORM_MMC_FLUSH_DONE_CLR  0xa0000020
#define PLATFORM_MMC_FLUSH_DONE_BIT  0x1
#define PLATFORM_MMC_FLUSH_TIMEOUT   10000000

static struct plic_data plic = {
    .addr = PLATFORM_PLIC_ADDR,
    .size = PLATFORM_PLIC_SIZE,
    .num_src = PLATFORM_PLIC_NUM_SOURCES,
	.context_map = {
		[0] = { 0, 1 },
	},
};

static struct aclint_mswi_data mswi = {
    .addr = PLATFORM_ACLINT_MSWI_ADDR,
    .size = ACLINT_MSWI_SIZE,
    .first_hartid = 0,
    .hart_count = PLATFORM_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
    .mtime_freq = PLATFORM_ACLINT_MTIMER_FREQ,
    .mtime_addr = PLATFORM_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIME_OFFSET,
    .mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
    .mtimecmp_addr = PLATFORM_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIMECMP_OFFSET,
    .mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
    .first_hartid = 0,
    .hart_count = PLATFORM_HART_COUNT,
    .has_64bit_mmio = false,
};

static bool mmc_flush_enabled;

static int rvcpu_system_reset_check(u32 type, u32 reason)
{
    return 1;
}

static bool rvcpu_mmc_flush_enabled_from_fdt(void)
{
    const void *fdt = fdt_get_address();
    int nodeoff = -1;

    if (!fdt || fdt_check_header(fdt))
        return false;

    while (1) {
        nodeoff = fdt_node_offset_by_compatible(fdt, nodeoff, "rvcom,sdcard");
        if (nodeoff < 0)
            break;
        if (fdt_node_is_enabled(fdt, nodeoff))
            return true;
    }

    return false;
}

static void rvcpu_mmc_cache_flush(void)
{
    u32 i;

    writel(1, (void *)PLATFORM_MMC_FLUSH_ADDR);
    for (i = 0; i < PLATFORM_MMC_FLUSH_TIMEOUT; i++) {
        if (readl((void *)PLATFORM_MMC_FLUSH_DONE_ADDR) &
            PLATFORM_MMC_FLUSH_DONE_BIT) {
            writel(1, (void *)PLATFORM_MMC_FLUSH_DONE_CLR);
            break;
        }
    }
}

static void rvcpu_system_reset(u32 type, u32 reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
		if (mmc_flush_enabled)
			rvcpu_mmc_cache_flush();
		csr_write(CSR_MIE, 0);
		while (1)
			wfi();
		break;

	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
	default:
		writel(1, (void *)PLATFORM_RST_CTRL_ADDR);
		while (1)
			;
		break;
	}
}

static struct sbi_system_reset_device rvcpu_reset = {
    .name = "rvcpu_reset",
    .system_reset_check = rvcpu_system_reset_check,
    .system_reset = rvcpu_system_reset,
};

/*
 * Platform early initialization.
 */
static int platform_early_init(bool cold_boot)
{
    int rc;
    
    if (!cold_boot)
        return 0;

    mmc_flush_enabled = rvcpu_mmc_flush_enabled_from_fdt();
    sbi_system_reset_add_device(&rvcpu_reset);

    rc = rvcpu_uart_init(PLATFORM_UART_ADDR);
    if (rc)
        return rc;

    return aclint_mswi_cold_init(&mswi);
}

/*
 * Platform final initialization.
 */
static int platform_final_init(bool cold_boot)
{
    return 0;
}

/*
 * Initialize the platform interrupt controller during cold boot.
 */
static int platform_irqchip_init(void)
{
    /* Example if the generic PLIC driver is used */
    return plic_cold_irqchip_init(&plic);
}

/*
 * Initialize platform timer during cold boot.
 */
static int platform_timer_init(void)
{
    /* Example if the generic ACLINT driver is used */
    return aclint_mtimer_cold_init(&mtimer, NULL);
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
    .early_init         = platform_early_init,
    .final_init         = platform_final_init,
    .irqchip_init       = platform_irqchip_init,
    .timer_init         = platform_timer_init
};

const struct sbi_platform platform = {
    .opensbi_version    = OPENSBI_VERSION,
    .platform_version   = SBI_PLATFORM_VERSION(0x0, 0x00),
    .name               = "isct,rvcpu",
    .features           = SBI_PLATFORM_DEFAULT_FEATURES,
    .hart_count         = 1,
    .hart_stack_size    = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
    .heap_size          = SBI_PLATFORM_DEFAULT_HEAP_SIZE(1),
    .platform_ops_addr  = (unsigned long)&platform_ops
};
