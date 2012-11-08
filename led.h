#ifndef __LED_H__
#define __LED_H__

#define LED_GPIO_PORT 0

typedef enum
{
	RED = 0,
	GREEN = 1,
	BLUE = 3,
} LED;

void LED_Init();
void LED_On(LED led);
void LED_Off(LED led);
void LED_Toggle(LED led);

#endif