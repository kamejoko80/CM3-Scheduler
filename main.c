/*

Main file for SLIP D embedded software

*/

/* includes */
#include "efm32.h"

#include "efm32_chip.h"
#include "efm32_int.h"
#include "efm32_cmu.h"
#include "efm32_rtc.h"
#include "efm32_timer.h"

#include <stdlib.h>

#include "led.h"
#include "trace.h"
#include "tasker.h"

/* variables */



/* functions */
void wait(uint32_t ms)
{
	
	uint32_t time, 
		clockFreq = CMU_ClockFreqGet(cmuClock_RTC);
	
	while (ms > 0)
	{
		
		time = RTC_CounterGet();
		
		if (16777215 - time < ((double)ms / 1000.0) * clockFreq)
		{
			ms -= (uint32_t)(1000.0 * ((16777215 - time) / (double)clockFreq));
			while (RTC_CounterGet() > time);
		}
		else
		{
			while (RTC_CounterGet() < time + ((double)ms / 1000.0) * clockFreq);
			break;
		}
		
	}
	
}

void startupLEDs()
{
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(1000);
	
	LED_On(RED);
	LED_On(BLUE);
	LED_On(GREEN);
	
	wait(1000);
	
	LED_Off(RED);
	LED_Off(BLUE);
	LED_Off(GREEN);
	
	wait(1000);
	
}

void InitClocks()
{
	/* Starting LFXO and waiting until it is stable */
  CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
  
  // starting HFXO, wait till stable
  CMU_OscillatorEnable(cmuOsc_HFXO, true, true);
	
	// route HFXO to CPU
	CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
	
  /* Routing the LFXO clock to the RTC */
  CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
  CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);
  
  // disabling the RCs
	CMU_ClockEnable(cmuSelect_HFRCO, false);
	CMU_ClockEnable(cmuSelect_LFRCO, false);

  /* Enabling clock to the interface of the low energy modules */
  CMU_ClockEnable(cmuClock_CORE, true);
  CMU_ClockEnable(cmuClock_CORELE, true);
  
  // enable clock to hf perfs
  CMU_ClockEnable(cmuClock_HFPER, true);
	
	// enable clock to GPIO
	CMU_ClockEnable(cmuClock_GPIO, true);
	
	// enable clock to RTC
	CMU_ClockEnable(cmuClock_RTC, true);
	RTC_Reset();
	RTC_Enable(true);
	
	// enable clock to USARTs
	CMU_ClockEnable(cmuClock_UART1, true);
	
}

void enableInterrupts()
{
	
	/* Enable TIMER0 interrupt vector in NVIC */
  NVIC_EnableIRQ(SysTick_IRQn);
  NVIC_EnableIRQ(PendSV_IRQn);
  
  NVIC_EnableIRQ(UART1_TX_IRQn);
	
}

void enableTimers()
{
	
	
}

uint8_t context_flash_blue_stack[1024];
void context_flash_blue()
{
	
	while(1)
	{
		TRACE("TASK 1!\n");
		wait(10);
	}
	
}

uint8_t context_flash_green_stack[1024];
void context_flash_green()
{
	
	while(1)
	{
		TRACE("TASK 2!\n");
		wait(10);
	}
	
}

int main()
{
	
	// Chip errata
	CHIP_Init();
	
	// ensure core frequency has been updated
	SystemCoreClockUpdate();
	
	// start clocks
	InitClocks();
		
	// init LEDs
	LED_Init();
	
	// init trace
	TRACE_Init();
	
	// show startup LEDs
	startupLEDs();
	
	enableTimers();
	
	SysTick_Config(500000); // ~ 2ms
	
	// init tasks innit
	if (!task_init(context_flash_blue, context_flash_blue_stack, 1024))
	{
		while(1);
	}
	if (!task_init(context_flash_green, context_flash_green_stack, 1024))
	{
		while(1);
	}
	
	enableInterrupts();
	
	tasker_start();
	
	while(1);
	
}
