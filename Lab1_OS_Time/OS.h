#ifndef __OS_H__
#define __OS_H__

#define OS_MAX_TASKS 16

void Timer2_Init(void);
int OS_Add_Periodic_Thread(void(*task)(void), unsigned long period, unsigned long priority);
#endif
