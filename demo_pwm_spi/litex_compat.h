#ifndef LITEX_COMPAT_H
#define LITEX_COMPAT_H

#include <stdint.h>
#include <stdio.h>

#include <generated/csr.h>
#include <generated/soc.h>
#include <irq.h>
#include <libbase/uart.h>

#ifdef CONFIG_CPU_TYPE_NEORV32
#define NEORV32_CLINT_BASE 0xfff40000u
#define NEORV32_SYSINFO   ((litex_sysinfo_t *)0xfffe0000u)
#endif
#define LITEX_UART0 ((void *)0)

#define CSR_MTVEC  0x305
#define CSR_MIE    0x304
#define CSR_MCAUSE 0x342
#define CSR_MEPC   0x341

#ifdef CONFIG_CPU_TYPE_NEORV32
#define CSR_MIE_MTIE (1u << 7)

typedef struct {
	const uint32_t MISC;
	const uint32_t SOC;
	const uint32_t CACHE;
	uint32_t CLK;
} litex_sysinfo_t;
#else
#define CSR_MIE_MEIE (1u << 11)
#endif

static inline uint32_t litex_csr_read(unsigned int csr)
{
	uint32_t value;

	switch (csr) {
	case CSR_MCAUSE:
		asm volatile ("csrr %0, mcause" : "=r"(value));
		break;
	case CSR_MEPC:
		asm volatile ("csrr %0, mepc" : "=r"(value));
		break;
	case CSR_MIE:
		asm volatile ("csrr %0, mie" : "=r"(value));
		break;
	default:
		value = 0;
		break;
	}

	return value;
}

static inline void litex_csr_write(unsigned int csr, uint32_t value)
{
	switch (csr) {
	case CSR_MTVEC:
		asm volatile ("csrw mtvec, %0" :: "r"(value));
		break;
	case CSR_MIE:
		asm volatile ("csrw mie, %0" :: "r"(value));
		break;
	default:
		break;
	}
}

static inline void litex_csr_set(unsigned int csr, uint32_t mask)
{
	if (csr == CSR_MIE)
		asm volatile ("csrs mie, %0" :: "r"(mask));
}

static inline void litex_leds_write(uint32_t value)
{
#ifdef CSR_LEDS_BASE
	leds_out_write(value);
#else
	(void)value;
#endif
}

static inline void litex_led_toggle(unsigned int pin)
{
#ifdef CSR_LEDS_BASE
	static uint32_t leds;

	leds ^= (1u << pin);
	leds_out_write(leds);
#else
	(void)pin;
#endif
}

static inline void litex_uart_setup(void *uart, uint32_t baudrate, uint32_t irq_mask)
{
	(void)uart;
	(void)baudrate;
	(void)irq_mask;
	uart_init();
}

#define litex_cpu_csr_read(csr)        litex_csr_read(csr)
#define litex_cpu_csr_write(csr, val)  litex_csr_write(csr, val)
#define litex_cpu_csr_set(csr, mask)   litex_csr_set(csr, mask)
#define litex_cpu_sleep()              __asm volatile ("wfi")

#define litex_gpio_port_set(value)     litex_leds_write(value)
#define litex_gpio_pin_toggle(pin)     litex_led_toggle(pin)

#define litex_uart_puts(uart, string)              fputs((string), stdout)
#define litex_uart_printf(uart, ...)               printf(__VA_ARGS__)

#ifdef CONFIG_CPU_TYPE_NEORV32
#define clint_available() 1
#define gptmr_available() 0
#endif

#endif
