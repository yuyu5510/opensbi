/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Archlab, Science Tokyo
 * 
 * Authors:
 *   Archlab, Science Tokyo
 */

#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/serial/rvcpu-uart.h>

static int serial_rvcpu_init(const void *fdt, int nodeoff,
			     const struct fdt_match *match)
{
	uint64_t reg_addr, reg_size;
	int rc;

	if (nodeoff < 0 || !fdt)
		return SBI_ENODEV;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &reg_addr, &reg_size);
	if (rc < 0 || !reg_addr || !reg_size)
		return SBI_ENODEV;

	return rvcpu_uart_init(reg_addr);
}

static const struct fdt_match serial_rvcpu_match[] = {
	{ .compatible = "isct,rvcpu-uart" },
	{ },
};

const struct fdt_serial fdt_serial_rvcpu = {
	.match_table = serial_rvcpu_match,
	.init = serial_rvcpu_init
};
