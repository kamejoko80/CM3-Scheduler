/*

Main file for SLIP D embedded software

*/

/* includes */
#include "efm32.h"

#include "efm32.h"
#include "efm32_chip.h"
#include "efm32_int.h"
#include "efm32_cmu.h"
#include "efm32_rtc.h"
#include "efm32_timer.h"

#include <stdlib.h>

#include "led.h"
#include "trace.h"

/* variables */
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
	
	CMU_ClockEnable(cmuClock_TIMER0, true);
	
}

void enableInterrupts()
{
	
	/* Enable TIMER0 interrupt vector in NVIC */
  NVIC_EnableIRQ(SysTick_IRQn);
  NVIC_EnableIRQ(PendSV_IRQn);
	
}

void enableTimers()
{
	
	
}

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
} hw_stack_frame_t;

uint8_t new_stack[1024];
void newContext(uint32_t arg)
{
	
	INT_Disable();
	
	LED_On(RED);
	LED_On(GREEN);
	LED_On(BLUE);
	
	TRACE("NEW CONTEXT!\n");
	
	char tmsg[255];
	sprintf(tmsg,"arg: %i\n", arg);
	TRACE(tmsg);
	
	while(1);
}

void newContextDeath()
{
	TRACE("DYING!\n");
	while(1);
}

void PendSV_Handler()
{
	
	uint32_t sp = ((uint32_t)new_stack) + 1024 - sizeof(hw_stack_frame_t);
	
	hw_stack_frame_t *process_frame = (hw_stack_frame_t*)(sp);
	process_frame->r0 = 12345;
	process_frame->r1 = 0;
	process_frame->r2 = 0;
	process_frame->r3 = 0;
	process_frame->r12 = 0;
	process_frame->pc = (uint32_t)newContext;
	process_frame->lr = (uint32_t)newContextDeath;
	process_frame->psr = 0x21000000; //default PSR value
	
	__asm volatile (
		
		"MSR PSP, %0\n\t"
		"ORR lr, lr, #4\n\t"
		: 
		: "r" (sp)
		
	);
	
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
	TRACE("Trace started\n");
		
	// show startup LEDs
	startupLEDs();
	
	enableTimers();
	
	NVIC_EnableIRQ(PendSV_IRQn);
	TRACE("about to switch context\n");
	SCB->ICSR |= (1 << 28);
	
	TRACE("and we're back!!");
	
	while(1)
	{
		LED_On(GREEN);
		wait(500);
	}
	
}
