// OLEDTestMain.c
// Runs on LM3S8962
// Test OutputD.c by sending various characters and strings to
// the OLED display and verifying that the output is correct.
// Daniel Valvano
// July 28, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
   Section 3.4.5

 Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include <stdio.h>
#include "Output.h"

// delay function for testing from sysctl.c
// which delays 3*ulCount cycles
#ifdef __TI_COMPILER_VERSION__
	//Code Composer Studio Code
	void Delay(unsigned long ulCount){
	__asm (	"    subs    r0, #1\n"
			"    bne     Delay\n"
			"    bx      lr\n");
}

#else
	//Keil uVision Code
	__asm void
	Delay(unsigned long ulCount)
	{
    subs    r0, #1
    bne     Delay
    bx      lr
	}

#endif
int main(void){
  int i;
  Output_Init();
  Output_Color(15);
  printf("Hello, world.");
  printf("%c", NEWLINE);
  Delay(4000000);           // delay ~1 sec at 12 MHz
  Output_Color(8);
  printf("A really long string should go to the next line.\r");
  Delay(4000000);           // delay ~1 sec at 12 MHz
  printf("Oxxx(:::::::::::::::>%c", NEWLINE);
  Delay(4000000);           // delay ~1 sec at 12 MHz
  Output_Color(15);
  printf("Color Table:%c", NEWLINE);
  Delay(4000000);           // delay ~1 sec at 12 MHz
  Output_Color(8);
  printf("<:::::::::::::::)xxxO%c", NEWLINE);
  for(i=15; i>=1; i=i-2){
    Delay(4000000);         // delay ~1 sec at 12 MHz
    Output_Color(i);
    printf("Color: %u%c", i, TAB);
    Output_Color(i-1);
    printf("Color: %u%c", i-1, NEWLINE);
  }
  Delay(4000000);           // delay ~1 sec at 12 MHz
  Output_Clear();
  while(1){};
}
