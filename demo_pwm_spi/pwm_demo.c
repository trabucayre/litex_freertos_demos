/*
 * Simple FreeRTOS PWM demo for LiteX.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libbase/console.h>

#include "litex_compat.h"

#define UART_HW_HANDLE          ( LITEX_UART0 )
#define PROMPT_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )
#define PROMPT_TASK_STACK_WORDS ( configMINIMAL_STACK_SIZE + 128 )
#define INPUT_BUFFER_SIZE       ( 48 )

void pwm_demo(void);

static void prvPwmTask(void *pvParameters);
static char *readstr(void);
static char *get_token(char **str);
static void prvHandleCommand(char *str);
static void prvHandlePwmCommand(char *str);
static void prvHandleSPICommand(char *str);
static int prvParseUnsigned(const char *text, uint32_t *value);

extern void vToggleLED(void);

void pwm_demo(void)
{
	BaseType_t created;

	created = xTaskCreate(prvPwmTask,
			      "PWM",
			      PROMPT_TASK_STACK_WORDS,
			      NULL,
			      PROMPT_TASK_PRIORITY,
			      NULL);
	configASSERT(created == pdPASS);

	vTaskStartScheduler();

	for (;;)
		;
}

static void prvPwmTask(void *pvParameters)
{
	char *command;

	(void)pvParameters;

	litex_uart_puts(UART_HW_HANDLE, "PWM demo ready.\n");
	litex_uart_puts(UART_HW_HANDLE, "Commands: help, led, pwm en 0|1, pwm p <value>, pwm d <value>, spi cs 0|1, spi w <value>\n\n");
	litex_uart_puts(UART_HW_HANDLE, "prompt> ");

	for (;;) {
		command = readstr();
		if (command == NULL) {
			vTaskDelay(pdMS_TO_TICKS(10));
			continue;
		}

		prvHandleCommand(command);
		litex_uart_puts(UART_HW_HANDLE, "prompt> ");
	}
}

static void prvHandleCommand(char *str)
{
	char *command = get_token(&str);

	if (strcmp(command, "help") == 0) {
		litex_uart_puts(UART_HW_HANDLE,
				"help          - show commands\n"
				"led           - toggle LED 0\n"
				"pwm en 0|1    - disable or enable PWM\n"
				"pwm p <value> - set PWM period\n"
				"pwm d <value> - set PWM duty/width\n"
				"spi cs 0|1    - disable or enable SPI\n"
				"spi w <value> - send SPI\n");
	} else if (strcmp(command, "led") == 0) {
		vToggleLED();
		litex_uart_puts(UART_HW_HANDLE, "LED toggled.\n");
	} else if (strcmp(command, "pwm") == 0) {
		prvHandlePwmCommand(str);
	} else if (strcmp(command, "spi") == 0) {
		prvHandleSPICommand(str);
	} else if (command[0] == '\0') {
		litex_uart_puts(UART_HW_HANDLE, "Type 'help' for commands.\n");
	} else {
		litex_uart_puts(UART_HW_HANDLE, "Unknown command. Type 'help'.\n");
	}
}

static void prvHandlePwmCommand(char *str)
{
	char *subcommand = get_token(&str);
	char *argument = get_token(&str);
	uint32_t value;

#ifndef CSR_PWM_BASE
	(void)subcommand;
	(void)argument;
	litex_uart_puts(UART_HW_HANDLE, "PWM is not present in this SoC build.\n");
#else
	if (strcmp(subcommand, "en") == 0) {
		if (!prvParseUnsigned(argument, &value) || (value > 1u)) {
			litex_uart_puts(UART_HW_HANDLE, "Usage: pwm en 0|1\n");
			return;
		}

		pwm_enable_write(value);
		litex_uart_printf(UART_HW_HANDLE, "PWM enable = %lu\n", value);
	} else if (strcmp(subcommand, "p") == 0) {
		if (!prvParseUnsigned(argument, &value)) {
			litex_uart_puts(UART_HW_HANDLE, "Usage: pwm p <value>\n");
			return;
		}

		pwm_period_write(value);
		litex_uart_printf(UART_HW_HANDLE, "PWM period = %lu\n", value);
	} else if (strcmp(subcommand, "d") == 0) {
		if (!prvParseUnsigned(argument, &value)) {
			litex_uart_puts(UART_HW_HANDLE, "Usage: pwm d <value>\n");
			return;
		}

		pwm_width_write(value);
		litex_uart_printf(UART_HW_HANDLE, "PWM duty/width = %lu\n", value);
	} else {
		litex_uart_puts(UART_HW_HANDLE,
				"Usage: pwm en 0|1, pwm p <value>, pwm d <value>\n");
	}
#endif
}

static void prvHandleSPICommand(char *str)
{
	char *subcommand = get_token(&str);
	char *argument = get_token(&str);
	uint32_t value;

#ifndef CSR_SPI_BASE  // See ../build/olimex_gatemate_a1_evb/software/include/generated/csr.h
	(void)subcommand;
	(void)argument;
	litex_uart_puts(UART_HW_HANDLE, "SPI is not present in this SoC build.\n");
#else
	if (strcmp(subcommand, "cs") == 0) {
		if (!prvParseUnsigned(argument, &value) || (value > 1u)) {
			litex_uart_puts(UART_HW_HANDLE, "Usage: spi cs 0|1\n");
			return;
		}

		spi_cs_write(value);
		spi_control_write(8*(1<<8)|1<<0);
		litex_uart_printf(UART_HW_HANDLE, "SPI CS = %lu\n", value);
	} else if (strcmp(subcommand, "w") == 0) {
		if (!prvParseUnsigned(argument, &value)) {
			litex_uart_puts(UART_HW_HANDLE, "Usage: spi w <value>\n");
			return;
		}

		spi_mosi_write(value);
		spi_control_write(8*(1<<8)|1<<0);
		litex_uart_printf(UART_HW_HANDLE, "SPI MOSI = %lu\n", value);
	} else {
		litex_uart_puts(UART_HW_HANDLE,
				"Usage: spi w <value>\n");
	}
#endif
}

static char *readstr(void)
{
	char c[2];
	static char s[INPUT_BUFFER_SIZE];
	static int ptr = 0;

	if (readchar_nonblock()) {
		c[0] = getchar();
		c[1] = 0;
		switch (c[0]) {
		case 0x7f:
		case 0x08:
			if (ptr > 0) {
				ptr--;
				fputs("\x08 \x08", stdout);
			}
			break;
		case 0x07:
			break;
		case '\r':
		case '\n':
			s[ptr] = 0x00;
			fputs("\n", stdout);
			ptr = 0;
			return s;
		default:
			if (ptr >= ((int)sizeof(s) - 1))
				break;
			fputs(c, stdout);
			s[ptr] = c[0];
			ptr++;
			break;
		}
	}

	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if (c == NULL) {
		d = *str;
		*str = *str + strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c + 1;
	return d;
}

static int prvParseUnsigned(const char *text, uint32_t *value)
{
	char *end;
	unsigned long parsed;

	if ((text == NULL) || (*text == '\0') || (*text == '-'))
		return 0;

	parsed = strtoul(text, &end, 0);
	if ((end == text) || (*end != '\0'))
		return 0;

	*value = (uint32_t)parsed;
	return 1;
}
