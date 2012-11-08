#include "led.h"

#include "efm32_gpio.h"

void LED_Init()
{
	// set pins as output
	GPIO_PinModeSet(gpioPortA, 0, gpioModeWiredAnd, 1);
	GPIO_PinModeSet(gpioPortA, 1, gpioModeWiredAnd, 1);
	GPIO_PinModeSet(gpioPortA, 3, gpioModeWiredAnd, 1);
}

void LED_On(LED led)
{
	GPIO->P[LED_GPIO_PORT].DOUT &= ~(1 << led);
}

void LED_Off(LED led)
{
	GPIO->P[LED_GPIO_PORT].DOUT |= (1 << led);
}

void LED_Toggle(LED led)
{
	GPIO->P[LED_GPIO_PORT].DOUT ^= (1 << led);
}
