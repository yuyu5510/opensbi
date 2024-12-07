/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Archlab, Science Tokyo
 * 
 * Authors:
 *   Archlab, Science Tokyo
 */

#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/serial/rvcpu-uart.h>

#define UART_REG_RXTX		0
#define UART_REG_TXFULL		1
#define UART_REG_RXEMPTY	2

static volatile u32 *uart_base;

static u8 get_reg(u8 reg)
{
	return readb(uart_base + reg);
}

static void set_reg(u8 reg, u8 val)
{
	writeb(val, uart_base + reg);
}

static void rvcpu_uart_putc(char ch)
{
	while (get_reg(UART_REG_TXFULL));
	set_reg(UART_REG_RXTX, ch);
}

static int rvcpu_uart_getc(void)
{
	if (get_reg(UART_REG_RXEMPTY))
		return -1;
	return get_reg(UART_REG_RXTX);
}

static struct sbi_console_device rvcpu_console = {
	.name = "rvcpu_uart",
	.console_putc = rvcpu_uart_putc,
	.console_getc = rvcpu_uart_getc
};

int rvcpu_uart_init(unsigned long base)
{
	uart_base = (volatile u32 *)base;
	sbi_console_set_device(&rvcpu_console);
	return 0;
}
