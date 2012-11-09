#include "tasker.h"

#include "efm32.h"
#include "efm32_int.h"

task_table_t task_table[MAX_TASKS];
uint32_t current_task = MAX_TASKS-1;
bool msp_in_use = true;

void task_switch(uint32_t flags, bool exec)
{
	
	flags |= IN_USE_FLAG;
	
	if (exec)
		flags |= EXEC_FLAG;
	else
		flags &= ~EXEC_FLAG;
	
	INT_Disable();
	task_table[current_task].flags = flags;
	INT_Enable();
	
	SCB->ICSR |= (1 << 28);
	
}

void tasker_release(uint32_t flags)
{
	
	INT_Disable();
	int i;
	for (i = 0; i < MAX_TASKS; i++)
	{
		if (task_table[i].flags & flags)
		{
			task_table[i].flags &= (~flags);
			task_table[i].flags |= EXEC_FLAG;
		}
	}
	INT_Enable();
	
}

uint32_t tasker_getCurrentFlags()
{
	return task_table[current_task].flags;
}

uint32_t tasker_getCurrentTask()
{
	return current_task;
}

void task_return()
{
	
	task_table[current_task].flags = 0;
	
	SCB->ICSR |= (1 << 28);
	
	while(1);
	
}

void SysTick_Handler()
{
	
	if (!msp_in_use)
		__asm volatile (
			"STMIA %1!, {r4-r11}\n\t"
			"MRS %0, PSP\n\t"
				: "=r" (task_table[current_task].sp)  // store current PSP in SP
				: "r" (&task_table[current_task].sw_stack_frame)
			);
	
	msp_in_use = false;
	
	do
	{
		
		current_task = (current_task + 1) % MAX_TASKS;
		
		if (task_table[current_task].flags & (EXEC_FLAG | IN_USE_FLAG))
		{
			
			break;
			
		}
		
	}
	while(1);
	
	__asm volatile(
		"mov lr, #0xFFFFFFFD\n\t"
		"LDMIA %0!, {r4-r11}\n\t"
		"MSR PSP, %1\n\t"
		:
		: "r" (&task_table[current_task].sw_stack_frame),
			"r" (task_table[current_task].sp)
	);
	
}

void PendSV_Handler()
{
	
	if (!msp_in_use)
		__asm volatile (
			"STMIA %1!, {r4-r11}\n\t"
			"MRS %0, PSP\n\t"
				: "=r" (task_table[current_task].sp)  // store current PSP in SP
				: "r" (&task_table[current_task].sw_stack_frame)
			);
	
	msp_in_use = false;
	
	do
	{
		
		current_task = (current_task + 1) % MAX_TASKS;
		
		if (task_table[current_task].flags & (EXEC_FLAG | IN_USE_FLAG))
		{
			
			break;
			
		}
		
	}
	while(1);
	
	__asm volatile(
		"mov lr, #0xFFFFFFFD\n\t"
		"LDMIA %0!, {r4-r11}\n\t"
		"MSR PSP, %1\n\t"
		:
		: "r" (&task_table[current_task].sw_stack_frame),
			"r" (task_table[current_task].sp)
	);
	
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

void tasker_init()
{
	int i;
	for (i = 0; i < MAX_TASKS; i++)
	{
		task_table[i].flags = 0;
	}
}

void tasker_start()
{
	task_switch(0,true);
}
