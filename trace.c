#include "trace.h"

#include <string.h>
#include <stdbool.h>

#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_int.h"
#include "efm32_usart.h"

#include "tasker.h"
#include "led.h"

static uint32_t uart1_owner = UART_OWNER_NONE;
static uint8_t *uart1_payload,
	uart1_position,
	uart1_length;

void UART1_TX_IRQHandler()
{
	
	if (!(UART1->STATUS & UART_STATUS_TXBL))
		return;
	
	if (uart1_position < uart1_length)
	{
		UART1->TXDATA = uart1_payload[uart1_position];
		uart1_position++;
	}
	else
	{
		// int disable
		INT_Disable();
		// release lock
		uart1_owner = UART_OWNER_NONE;
		// update flags
		tasker_release(UART1_LOCK);
		// disable irq
		USART_IntDisable(UART1, UART_IF_TXBL);
		// int enable
		INT_Enable();
	}
	
}

void TRACE_Init()
{
	
	UART1->CLKDIV = 256 * ((CMU_ClockFreqGet(cmuClock_HF)) / (115200 * 16) - 1);
	
	UART1->CMD = UART_CMD_TXEN | UART_CMD_RXEN;
	
	GPIO->P[4].DOUT |= (1 << 2);
	GPIO->P[4].MODEL =
						GPIO_P_MODEL_MODE2_PUSHPULL
					| GPIO_P_MODEL_MODE3_INPUT;
	
	UART1->ROUTE = UART_ROUTE_LOCATION_LOC3
          | UART_ROUTE_TXPEN | UART_ROUTE_RXPEN;
	
}

void TRACE(char* msg)
{
	
	do
	{
		INT_Disable();
		if (uart1_owner == UART_OWNER_NONE)
		{
			uart1_owner = tasker_getCurrentTask();
			INT_Enable();
			break;
		}
		INT_Enable();
		
		// yield
		task_switch(UART1_LOCK,false);
		
	}
	while (uart1_owner != tasker_getCurrentTask());
	
	uart1_payload = (uint8_t*)msg;
	uart1_position = 0,
	uart1_length = strlen(msg);
	
	// enable irq
	USART_IntEnable(UART1, UART_IF_TXBL);
	
	// yield
	task_switch(UART1_LOCK,false);
	
}