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

#include "led.h"

/* variables */
#define MAX_TASTS 32
typedef struct
{
	void *sp;
	int flags;
} task_table_t;
int current_task;
task_table_t task_table[MAX_TASKS];

#define MAIN_RETURN 0xFFFFFFF9  //Tells the handler to return using the MSP
#define THREAD_RETURN 0xFFFFFFFD //Tells the handler to return using the PSP

static uint32_t *stack;

/* prototypes */
void InitClocks();
void startupLEDs();
void wait(uint32_t ms);
void enableTimers();
void enableInterrupts();

/* functions */
//Reads the main stack pointer
static inline void * rd_stack_ptr(void){
  void * result=NULL;
  asm volatile ("MRS %0, msp\n\t"
      //"MOV r0, %0 \n\t"
      : "=r" (result) );
  return result;
}
 
//This saves the context on the PSP, the Cortex-M3 pushes the other registers using hardware
static inline void save_context(void){
  uint32_t scratch;
  asm volatile ("MRS %0, psp\n\t"
      "STMDB %0!, {r4-r11}\n\t"
      "MSR psp, %0\n\t"  : "=r" (scratch) );
}
//This loads the context from the PSP, the Cortex-M3 loads the other registers using hardware
static inline void load_context(void){
  uint32_t scratch;
  asm volatile ("MRS %0, psp\n\t"
      "LDMFD %0!, {r4-r11}\n\t"
      "MSR psp, %0\n\t"  : "=r" (scratch) );
}



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
	
	INT_Disable();
	
	
	
	INT_Enable();
	
}

void enableTimers()
{
	
	
	
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
		
	// show startup LEDs
	startupLEDs();
		
	enableTimers();
	
	enableInterrupts();
	
	
	
	while(1);
	
}