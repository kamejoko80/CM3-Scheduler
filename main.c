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

/* variables */
#define MAX_TASKS 32
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

//This defines the stack frame that is saved  by the hardware
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
 
//This defines the stack frame that must be saved by the software
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
 
void context_switcher(void);

volatile static bool running = false;

#define EXEC_FLAG 0x01
#define IN_USE_FLAG 0x02


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
  __asm volatile ("MRS %0, msp\n\t"
      //"MOV r0, %0 \n\t"
      : "=r" (result) );
  return result;
}
 
//This saves the context on the PSP, the Cortex-M3 pushes the other registers using hardware
static inline void save_context(void){
  volatile uint32_t scratch;
  __asm volatile ("MRS %0, psp\n\t"
      "STMDB %0!, {r4-r11}\n\t"
      "MSR psp, %0\n\t"  : "=r" (scratch) );
}
 
//This loads the context from the PSP, the Cortex-M3 loads the other registers using hardware
static inline void load_context(void){
  volatile uint32_t scratch;
  __asm volatile ("MRS %0, psp\n\t"
      "LDMFD %0!, {r4-r11}\n\t"
      "MSR psp, %0\n\t"  : "=r" (scratch) );
}
 
//This reads the PSP so that it can be stored in the task table
static inline void * rd_thread_stack_ptr(void){
    void * result=NULL;
    __asm volatile ("MRS %0, psp\n\t" : "=r" (result) );
    return(result);
}
 
//This writes the PSP so that the task table stack pointer can be used again
static inline void wr_thread_stack_ptr(void * ptr){
    __asm volatile ("MSR psp, %0\n\t" : : "r" (ptr) );
}

//This is the context switcher
void context_switcher(void){
    task_table[current_task].sp = rd_thread_stack_ptr(); //Save the current task's stack pointer
    do {
        current_task++;
        if ( current_task == MAX_TASKS ){
            current_task = 0;
            *((uint32_t*)stack) = MAIN_RETURN; //Return to main process using main stack
            break;
  } else if ( task_table[current_task].flags & EXEC_FLAG ){ //Check to see if this task should be skipped
            //change to unprivileged mode
            *((uint32_t*)stack) = THREAD_RETURN; //Use the thread stack upon handler return
            break;
  }
    } while(1);
    wr_thread_stack_ptr( task_table[current_task].sp ); //write the value of the PSP to the new task
}

//This is called when the task returns
void del_process(void){
  task_table[current_task].flags = 0; //clear the in use and exec flags
  SCB->ICSR |= (1<<28); //switch the context
  while(1); //once the context changes, the program will no longer return to this thread
}

//The SysTick interrupt handler -- this grabs the main stack value then calls the context switcher
void SysTick_Handler()
{
	if (!running) return;
    save_context();  //The context is immediately saved
    stack = (uint32_t *)rd_stack_ptr();
    if ( SysTick->CTRL & (1<16) ){ //Indicates timer counted to zero
        context_switcher();
    }
    load_context(); //Since the PSP has been updated, this loads the last state of the new task
}
 
//This does the same thing as the SysTick handler -- it is just triggered in a different way
void PendSV_Handler(void){
	
	if (!running) return;
    save_context();  //The context is immediately saved
    stack = (uint32_t *)rd_stack_ptr();
    //core_proc_context_switcher();
    context_switcher();
    load_context(); //Since the PSP has been updated, this loads the last state of the new task
}

int new_task(void *(*p)(void*), void * arg, void * stackaddr, int stack_size){
    int i;
    hw_stack_frame_t * process_frame;
    //Disable context switching to support multi-threaded calls to this function
    INT_Disable();
    for(i=0; i <= MAX_TASKS; i++){
			
				if (i == MAX_TASKS)
					break;
			
        if( task_table[i].flags == 0 ){
            process_frame = (hw_stack_frame_t *)(stackaddr - sizeof(hw_stack_frame_t));
            process_frame->r0 = (uint32_t)arg;
            process_frame->r1 = 0;
            process_frame->r2 = 0;
            process_frame->r3 = 0;
            process_frame->r12 = 0;
            process_frame->pc = ((uint32_t)p);
            process_frame->lr = (uint32_t)del_process;
            process_frame->psr = 0x21000000; //default PSR value
            task_table[i].flags = IN_USE_FLAG | EXEC_FLAG;
            task_table[i].sp = stackaddr + 
                stack_size -
                sizeof(hw_stack_frame_t) - 
                sizeof(sw_stack_frame_t);
            break;
        }
    }
    INT_Enable();  //Enable context switching
    
    if (i == MAX_TASKS)
    {
			return -1;
    }
    
    return i;
    
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
	
	CMU_ClockEnable(cmuClock_TIMER0, true);
	
}

void enableInterrupts()
{
	
	/* Enable TIMER0 interrupt vector in NVIC */
  NVIC_EnableIRQ(SysTick_IRQn);
	
}

void enableTimers()
{
	
	
}

uint8_t task_main_stack[1024];
void* task_main(void *arg)
{
	
	LED_On(GREEN);
	while(1);
	
}

uint8_t task2_main_stack[1024];
void* task2_main(void *arg)
{
	
	LED_On(RED);
	while(1);
	
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
	
	int i;
	for(i=0; i < MAX_TASKS; i++)
    task_table[i].flags = 0;
	
	if (new_task(task_main, 0, task_main_stack, 1024) == -1)
	{
		LED_On(RED); while(1);
	}
	if (new_task(task2_main, 0, task2_main_stack, 1024) == -1)
	{
		LED_On(RED); while(1);
	}
	
	SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000);
	
	enableInterrupts();
	
	running = true;
	
	while(1)
	{
		LED_Toggle(BLUE);
		wait(500);
	}
	
}
