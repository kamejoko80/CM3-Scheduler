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

#define MAX_TASKS 2

#define IN_USE_FLAG 	0x00000001
#define EXEC_FLAG 		0x00000002

typedef struct {
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
} sw_stack_frame_t;

typedef struct
{
	
	void *sp;
	uint32_t flags;
	sw_stack_frame_t sw_stack_frame;
	
} task_table_t;
task_table_t task_table[MAX_TASKS];
uint32_t current_task = MAX_TASKS-1;
bool msp_in_use = true;

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

static inline void* task_save_context(void){
  volatile uint32_t scratch;
	__asm volatile (
		"MRS %0, PSP\n\t"
		"STMDB %0!, {r4-r11}\n\t"
		: "=r" (scratch)
	);
	return (void*)scratch;
}

static inline void task_load_context(void* stack_ptr){
  __asm volatile (
		"LDMIA %0!, {r4-r11}\n\t"
		"MSR PSP, %0\n\t"  : "=r" ((uint32_t)stack_ptr) 
  );
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

uint8_t context_flash_blue_stack[1024];
void context_flash_blue()
{
	
	while(1)
	{
		LED_Toggle(BLUE);
		wait(1000);
	}
	
}

uint8_t context_flash_green_stack[1024];
void context_flash_green()
{
	
	while(1)
	{
		LED_Toggle(GREEN);
		wait(500);
	}
	
}

void task_switch()
{
	TRACE("Switching tasks\n");
	SCB->ICSR |= (1 << 28);
	while(1);
}

void task_return()
{
	
	TRACE("TASK DYING\n");
	
	task_table[current_task].flags = 0;
	
	task_switch();
	
	while(1);
	
}

void SysTick_Handler()
{
	
	__asm volatile(
		"STMIA %0!, {r4-r11}\n\t"
		:
		: "r" (&task_table[current_task].sw_stack_frame)
	);
	
	__asm volatile (
			"MRS %0, PSP\n\t"
			: "=r" (task_table[current_task].sp) // store current PSP in SP
		);
	
	
	do
	{
		
		current_task = (current_task + 1) % MAX_TASKS;
		
		if (task_table[current_task].flags & (EXEC_FLAG | IN_USE_FLAG))
		{
			
			break;
			
		}
		
	}
	while(1);
	
	/*
	//hw_stack_frame_t *process_frame = (hw_stack_frame_t*)(((uint32_t)task_table[current_task].sp) + sizeof(sw_stack_frame_t));
	hw_stack_frame_t *process_frame = (hw_stack_frame_t*)(((uint32_t)task_table[current_task].sp));
	
	char tmsg[255];
	sprintf(tmsg,"\nREAD VALUES\nr0=%8.8x;\nr1=%8.8x;\nr2=%8.8x;\nr3=%8.8x;\nr12=%8.8x;\nlr=%8.8x;\npc=%8.8x;\npsr=%8.8x;\n", 
		process_frame->r0,
		process_frame->r1 ,
		process_frame->r2 ,
		process_frame->r3 ,
		process_frame->r12 ,
		process_frame->lr,
		process_frame->pc,
		process_frame->psr);
	TRACE(tmsg);
	
	process_frame->r0 = 0;
	process_frame->r1 = 1;
	process_frame->r2 = 2;
	process_frame->r3 = 3;
	process_frame->r12 = 12;
	process_frame->pc = (uint32_t)context_flash_blue;
	process_frame->lr = (uint32_t)task_return;
	process_frame->psr = 0x21000000; //default PSR value
	
	sprintf(tmsg,"\nSET VALUES\nr0=%8.8x;\nr1=%8.8x;\nr2=%8.8x;\nr3=%8.8x;\nr12=%8.8x;\nlr=%8.8x;\npc=%8.8x;\npsr=%8.8x;\n", 
		process_frame->r0,
		process_frame->r1 ,
		process_frame->r2 ,
		process_frame->r3 ,
		process_frame->r12 ,
		process_frame->lr,
		process_frame->pc,
		process_frame->psr);
	TRACE(tmsg);
	*/
	__asm volatile (
		"mov lr, #0xFFFFFFFD\n\t"
		);
	
	__asm volatile(
		"LDMIA %0!, {r4-r11}\n\t"
		:
		: "r" (&task_table[current_task].sw_stack_frame)
	);
	
	__asm volatile (
		"MSR PSP, %0\n\t"
		:
		: "r" (task_table[current_task].sp));
}

void PendSV_Handler()
{
	
	
	
	do
	{
		
		current_task = (current_task + 1) % MAX_TASKS;
		
		if (task_table[current_task].flags & (EXEC_FLAG | IN_USE_FLAG))
		{
			
			break;
			
		}
		
	}
	while(1);
	
	__asm volatile (
		"mov lr, #0xFFFFFFFD\n\t"
		);
	
	__asm volatile(
		"LDMIA %0!, {r4-r11}\n\t"
		:
		: "r" (&task_table[current_task].sw_stack_frame)
	);
	
	__asm volatile (
		"MSR PSP, %0\n\t"
		:
		: "r" (task_table[current_task].sp));
	
}

bool task_init(void (*entry_point)(void), void *stack, uint32_t stack_size)
{
	
	uint32_t sp = ((uint32_t)stack) + stack_size - sizeof(hw_stack_frame_t);
	
	hw_stack_frame_t *process_frame = (hw_stack_frame_t*)(sp);
	process_frame->r0 = 0;
	process_frame->r1 = 0;
	process_frame->r2 = 0;
	process_frame->r3 = 0;
	process_frame->r12 = 0;
	process_frame->pc = (uint32_t)entry_point;
	process_frame->lr = (uint32_t)task_return;
	process_frame->psr = 0x21000000; //default PSR value
	
	int i;
	bool slot_found = false;
	for (i = 0; i < MAX_TASKS; i++)
	{
		
		if (!(task_table[i].flags & IN_USE_FLAG))
		{
			
			task_table[i].sp = (void*)(sp);
			task_table[i].flags = IN_USE_FLAG | EXEC_FLAG;
			memset(&task_table[i].sw_stack_frame,0,sizeof(sw_stack_frame_t));
			slot_found = true;
			break;
			
		}
		
	}
	
	return slot_found;
	
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
	
	//SysTick_Config(16777215);
	//SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 2);
	SysTick_Config(500000); // ~ 2ms
	
	int i;
	for (i = 0; i < MAX_TASKS; i++)
	{
		task_table[i].flags = 0;
	}
	
	// init tasks innit
	if (!task_init(context_flash_blue, context_flash_blue_stack, 1024))
	{
		TRACE("Unable to add blue flash task\n"); while(1);
	}
	if (!task_init(context_flash_green, context_flash_green_stack, 1024))
	{
		TRACE("Unable to add green flash task\n"); while(1);
	}
	
	TRACE("Added tasks\n");
	
	enableInterrupts();
	
	task_switch();
	
	while(1);
	
}
