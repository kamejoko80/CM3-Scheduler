#ifndef __TRACE_H__
#define __TRACE_H__

#define UART_OWNER_NONE 0xFFFFFFFF

void TRACE_Init();
void TRACE(char* msg);

#endif