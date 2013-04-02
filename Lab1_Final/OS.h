#ifndef __OS_H__
#define __OS_H__

#define OS_MAX_TASKS 10

void Timer2A_Init(void);
int OS_Add_Periodic_Thread(void(*task)(void), unsigned long period, unsigned long priority);
void Timer2A_Handler(void);
unsigned int OS_MsTime(void);
void OS_ClearMsTime(void);
#endif
