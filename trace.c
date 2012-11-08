#include "trace.h"

#include "string.h"
#include "efm32_gpio.h"
#include "efm32_cmu.h"
#include "efm32_usart.h"

#include "led.h"

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
	
	int todo, bytesToSend = strlen(msg);
	for (todo = 0; todo < bytesToSend; todo++) {
		while (!(UART1->STATUS & USART_STATUS_TXBL));
		UART1->TXDATA = *msg++;
	}
	
	while (!(UART1->STATUS & USART_STATUS_TXC));
	
}