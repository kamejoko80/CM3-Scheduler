#include "radio_task.h"
#include "tasks.h"

#include "led.h"

task_t radio_task;

void radio_task_entrypoint()
{
	
	int i;
	LED_On(RED);
	while(1)
	{
		for (i = 0; i < 2000000; i++);
		LED_Toggle(RED);
	}
	
}
