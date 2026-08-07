/******************************************************************************
 * FreeRTOS Demo for a LiteX SoC
 ******************************************************************************
 * FreeRTOS Kernel V10.4.4
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 ******************************************************************************/

/* Standard libraries */
#include <stdint.h>

/* FreeRTOS kernel */
#include <FreeRTOS.h>
#include <task.h>

/* LiteX HAL */
#include "litex_compat.h"

/* Platform UART configuration */
#define UART_BAUD_RATE (19200)         // transmission speed
#define UART_HW_HANDLE (LITEX_UART0)   // use UART0 (primary UART)

/* External definitions */
extern void pwm_demo(void);                     // actual show-case application
extern void freertos_risc_v_trap_handler(void); // FreeRTOS core

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
 * within this file. See https://www.freertos.org/a00016.html */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);
void freertos_risc_v_application_interrupt_handler(void);
void freertos_risc_v_application_exception_handler(void);
#ifdef TIMER0_INTERRUPT
void vPortSetupTimerInterrupt(void);
#endif

/* Platform-specific prototypes */
void vToggleLED(void);
void vSendString(const char * pcString);
static void prvSetupHardware(void);


/******************************************************************************
 * Main function (should never return).
 ******************************************************************************/
int main( void ) {

  // setup hardware
  prvSetupHardware();

  // say hello
  litex_uart_printf(UART_HW_HANDLE, "\n<<< LiteX running FreeRTOS %s >>>\n\n", tskKERNEL_VERSION_NUMBER);

  // run actual application code
  pwm_demo();

  // we should never reach this
  litex_uart_printf(UART_HW_HANDLE, "WARNING! pwm_demo returned!\n");
  return -1;
}


//#################################################################################################
// LiteX-Specific
//#################################################################################################

/******************************************************************************
 * Setup the hardware for this demo.
 ******************************************************************************/
static void prvSetupHardware(void) {

  // ----------------------------------------------------------
  // CPU setup
  // ----------------------------------------------------------

  // install the FreeRTOS kernel trap handler
  litex_cpu_csr_write(CSR_MTVEC, (uint32_t)&freertos_risc_v_trap_handler);
#ifdef TIMER0_INTERRUPT
  litex_cpu_csr_set(CSR_MIE, CSR_MIE_MEIE);
#endif

  // ----------------------------------------------------------
  // Peripheral setup
  // ----------------------------------------------------------

  // clear GPIO.out port
  litex_gpio_port_set(0);

  // setup UART0 at default baud rate, no interrupts
  litex_uart_setup(UART_HW_HANDLE, UART_BAUD_RATE, 0);

  // ----------------------------------------------------------
  // Configuration checks
  // ----------------------------------------------------------

  // check clock frequency configuration
  uint32_t litex_clk_hz = (uint32_t)CONFIG_CLOCK_FREQUENCY;
  if (litex_clk_hz != (uint32_t)configCPU_CLOCK_HZ) {
    litex_uart_printf(UART_HW_HANDLE,
                        "WARNING! Incorrect 'configCPU_CLOCK_HZ' configuration!\n"
                        "FreeRTOS configCPU_CLOCK_HZ: %lu Hz\n"
                        "LiteX clock speed:           %lu Hz\n\n",
                        (uint32_t)configCPU_CLOCK_HZ, (uint32_t)litex_clk_hz);
  }
}


#ifdef TIMER0_INTERRUPT
/******************************************************************************
 * Configure LiteX timer0 as the FreeRTOS tick source.
 ******************************************************************************/
void vPortSetupTimerInterrupt(void) {

  uint32_t timer_reload = (uint32_t)(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

  timer0_en_write(0);
  timer0_ev_pending_write(1);
  timer0_load_write(timer_reload);
  timer0_reload_write(timer_reload);
  timer0_ev_enable_write(1);
  timer0_en_write(1);

  irq_setmask(irq_getmask() | (1u << TIMER0_INTERRUPT));
  litex_cpu_csr_set(CSR_MIE, CSR_MIE_MEIE);
}
#endif


/******************************************************************************
 * Handle LiteX/application-specific interrupts.
 ******************************************************************************/
void freertos_risc_v_application_interrupt_handler(void) {

#ifdef TIMER0_INTERRUPT
  uint32_t pending = irq_pending() & irq_getmask();

  if (pending & (1u << TIMER0_INTERRUPT)) {
    timer0_ev_pending_write(1);
    portYIELD_FROM_ISR(xTaskIncrementTick());
    return;
  }

#ifdef UART_INTERRUPT
  if (pending & (1u << UART_INTERRUPT)) {
    uart_isr();
    return;
  }
#endif
#else
  // mcause identifies the cause of the interrupt
  uint32_t pending = litex_cpu_csr_read(CSR_MCAUSE);
#endif

  litex_uart_printf(UART_HW_HANDLE, "\n<LiteX-IRQ> Unexpected IRQ! pending=0x%lx </LiteX-IRQ>\n", (uint32_t)pending);
}


/******************************************************************************
 * Handle LiteX/application-specific exceptions.
 ******************************************************************************/
void freertos_risc_v_application_exception_handler(void) {

  // mcause identifies the cause of the exception
  uint32_t mcause = litex_cpu_csr_read(CSR_MCAUSE);

  // mepc identifies the address of the exception
  uint32_t mepc = litex_cpu_csr_read(CSR_MEPC);

  // debug output
  litex_uart_printf(UART_HW_HANDLE, "\n<LiteX-EXC> mcause = 0x%lx @ mepc = 0x%lx </LiteX-EXC>\n", (uint32_t) mcause, (uint32_t)mepc); // debug output
}


/******************************************************************************
 * Toggle GPIO.out(0) pin.
 ******************************************************************************/
void vToggleLED(void) {

	litex_gpio_pin_toggle(0);
}


/******************************************************************************
 * Send a plain string via UART0.
 ******************************************************************************/
void vSendString(const char * pcString) {

	litex_uart_puts(UART_HW_HANDLE, (const char *)pcString);
}


/******************************************************************************
 * Assert terminator.
 ******************************************************************************/
void vAssertCalled(void) {

  int i;

	taskDISABLE_INTERRUPTS();

	/* Clear all LEDs */
  litex_gpio_port_set(0);

  litex_uart_puts(UART_HW_HANDLE, "FreeRTOS_FAULT: vAssertCalled called!\n");

	/* Flash the lowest 2 LEDs to indicate that assert was hit - interrupts are off
	here to prevent any further tick interrupts or context switches, so the
	delay is implemented as a busy-wait loop instead of a peripheral timer. */
	while(1) {
		for (i=0; i<(configCPU_CLOCK_HZ/100); i++) {
			__asm volatile( "nop" );
		}
		litex_gpio_pin_toggle(0);
		litex_gpio_pin_toggle(1);
	}
}


//#################################################################################################
// FreeRTOS Hooks
//#################################################################################################

/******************************************************************************
 * Hook for failing malloc.
 ******************************************************************************/
void vApplicationMallocFailedHook(void) {

	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created. It is also called by various parts of the
	demo application. The size of the heap available to pvPortMalloc() is
	defined by configTOTAL_HEAP_SIZE in FreeRTOSConfig.h. The
	xPortGetFreeHeapSize() API function can be used to query the size of free
	heap space that remains (although it does not provide information on how
	the remaining heap might be fragmented).

	If heap_3.c is used, then configTOTAL_HEAP_SIZE has no effect and the heap
	size is instead defined by a linker variable.
	xPortGetFreeHeapSize() cannot be used with heap_3.c. */

	taskDISABLE_INTERRUPTS();

  litex_uart_puts(UART_HW_HANDLE,
                    "FreeRTOS_FAULT: vApplicationMallocFailedHook "
                    "(increase 'configTOTAL_HEAP_SIZE' in FreeRTOSConfig.h)\n");

	__asm volatile("ebreak"); // trigger context switch

	while(1);
}


/******************************************************************************
 * Hook for the idle process.
 ******************************************************************************/
void vApplicationIdleHook(void) {

	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
	task. It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()). If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */

  litex_cpu_sleep(); // cpu wakes up on any interrupt request
}


/******************************************************************************
 * Hook for task stack overflow.
 ******************************************************************************/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {

	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook
	function is called if a stack overflow is detected. */

	taskDISABLE_INTERRUPTS();

  litex_uart_printf(UART_HW_HANDLE,
                      "FreeRTOS_FAULT: vApplicationStackOverflowHook "
                      "(increase 'configISR_STACK_SIZE_WORDS' in FreeRTOSConfig.h)\n");

	__asm volatile("ebreak"); // trigger context switch

	while(1);
}


/******************************************************************************
 * Hook for the application tick (unused).
 ******************************************************************************/
void vApplicationTickHook(void) {

  __asm volatile( "nop" ); // nothing to do here yet
}
