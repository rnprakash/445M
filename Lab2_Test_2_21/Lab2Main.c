#include <stdio.h>
#include "hw_types.h"
#include "sysctl.h"
#include "rit128x96x4.h"
#include "OS.h"
#include "UART.h"
#include "shell.h"

#define TIMESLICE               TIME_2MS    // thread switch time in system time units

void dummyTask1(void);
void dummyTask2(void);
void dummyTask3(void);

int main(void)
{
  OS_SemaphoreType* countingSemaphore;
	/* Initialize 8MHz clock */
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  OLED_Init(15);
  Timer2A_Init();
  OS_Init();
  SH_Init();
  
  // testing/debugging stuff
  OS_AddThread(&dummyTask1, 0, 1);
  OS_AddThread(&dummyTask2, 0, 2);
  OS_AddThread(&dummyTask3, 0, 3);
  OS_Debug();
//   OS_AddThread(&SH_Shell, 0, 3);
  OS_Launch(TIMESLICE);
	
	/* Loop indefinitely */
  while(1);
}

OS_SemaphoreType* binarySemaphore;
//int count1 = 0;
void dummyTask1(void) {
  int i;
  binarySemaphore = OS_InitSemaphore(OS_BINARY_SEMAPHORE);
  while(1) {
    OS_bWait(binarySemaphore);
    OLED_Out(TOP, "task 1 acquired semaphore");
    for(i = 0; i < 100000; i++)
      ;
    OS_bSignal(binarySemaphore);
    OLED_Out(TOP, "task 1 released semaphore");
    for(i = 0; i < 100000; i++)
      ;
  }
}

//int count2 = 0;
void dummyTask2(void) {
  int i;
  while(1) {
  OS_bWait(binarySemaphore);
    OLED_Out(BOTTOM, "task 2 acquired semaphore");
  for(i = 0; i < 100000; i++)
    ;
  OS_bSignal(binarySemaphore);
  OLED_Out(BOTTOM, "task 2 released semaphore");
  for(i = 0; i < 100000; i++)
    ;
	//OS_Kill();
  }
}

int count3 = 0;
void dummyTask3(void) {
  while(1) {
    count3++;
  }
}
