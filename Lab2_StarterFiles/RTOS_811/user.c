//*****************************************************************************
// Real Time Operating System 

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011

   Programs 6.4 through 6.12, section 6.2

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
 
#include "inc/hw_types.h"
#include "os.h"


#define TIMESLICE               TIME_2MS    // thread switch time in system time units


//
//*******************Initial TEST**********
// This is the simplest configuration, test this first
// run this with
// no UART interrupts
// no SYSTICK interrupts
// no timer interrupts
// no select interrupts
// no ADC serial port or oLED output
// no calls to semaphores
unsigned long Count1;   // number of times thread1 loops
unsigned long Count2;   // number of times thread2 loops
unsigned long Count3;   // number of times thread3 loops
#define GPIO_PORTD_DIR_R        (*((volatile unsigned long *)0x40007400))
#define GPIO_PORTD_DEN_R        (*((volatile unsigned long *)0x4000751C))
#define GPIO_PORTD1             (*((volatile unsigned long *)0x40007008))
#define GPIO_PORTD2             (*((volatile unsigned long *)0x40007010))
#define GPIO_PORTD3             (*((volatile unsigned long *)0x40007020))
#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
#define SYSCTL_RCGC2_GPIOD      0x00000008  // port D Clock Gating Control

#define GPIO_PORTD1     (*((volatile unsigned long *)0x40007008))
#define GPIO_PORTD2     (*((volatile unsigned long *)0x40007010))
#define GPIO_PORTD3     (*((volatile unsigned long *)0x40007020))
void Task1(void){
  Count1 = 0;
  for(;;){
    Count1++;
    GPIO_PORTD1 ^= 0x02;      // toggle PD1
  }
}
void Task2(void){
  Count2 = 0;
  for(;;){
    Count2++;
    GPIO_PORTD2 ^= 0x04;      // toggle PD2
  }
}
void Task3(void){
  Count3 = 0;
  for(;;){
    Count3++;
    GPIO_PORTD3 ^= 0x08;      // toggle PD3
  }
}
int main(void){  // testmain2
  OS_Init();           // initialize, disable interrupts
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOD; // activate port D
  GPIO_PORTD_DIR_R |= 0x0E;   // make PD3-1 out
  GPIO_PORTD_DEN_R |= 0x0E;   // enable digital I/O on PD3-1
  OS_AddThreads(&Task1, &Task2, &Task3);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
