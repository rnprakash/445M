#include <stdio.h>
#include "hw_types.h"
#include "sysctl.h"
#include "rit128x96x4.h"
#include "OS.h"
#include "UART.h"
#include "shell.h"

void EnableInterrupts(void);

int main(void)
{
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
	EnableInterrupts();
  OLED_Init(15);
  UART_Init();
	Timer2A_Init();
  SH_Init();
	//OS_Add_Periodic_Thread(&f, 2000, 5);
	
	/* Loop indefinitely */
  while(1) {
    ;
  }
}
