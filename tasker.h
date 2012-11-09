#ifndef __TASKER_H__
#define __TASKER_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_TASKS 2

#define IN_USE_FLAG 	0x00000001
#define EXEC_FLAG 		0x00000002
#define UART1_LOCK		0x00000004

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

typedef struct
{
	
	void *sp;
	uint32_t flags;
	sw_stack_frame_t sw_stack_frame;
	
} task_table_t;

void tasker_init();
void tasker_start();
void task_switch(uint32_t flags, bool exec);
void task_return();
bool task_init(void (*entry_point)(void), void *stack, uint32_t stack_size);
uint32_t tasker_getCurrentTask();
uint32_t tasker_getCurrentFlags();
void tasker_release(uint32_t flags);

#endif