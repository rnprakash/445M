// PeriodicSysTickInts.c
// Runs on LM3S8962
// Use the SysTick timer to request interrupts at a particular period.
// Daniel Valvano
// June 27, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011

   Program 5.12, section 5.7

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

// oscilloscope or LED connected to PD0 for period measurement
#include "hw_types.h"
#include "sysctl.h"
#include "lm3s8962.h"

//#define NVIC_SYS_PRI3_R         (*((volatile unsigned long *)0xE000ED20))  // Sys. Handlers 12 to 15 Priority
//#define NVIC_ST_CTRL_R          (*((volatile unsigned long *)0xE000E010))
//#define NVIC_ST_RELOAD_R        (*((volatile unsigned long *)0xE000E014))
//#define NVIC_ST_CURRENT_R       (*((volatile unsigned long *)0xE000E018))
//#define NVIC_ST_CTRL_CLK_SRC    0x00000004  // Clock Source
//#define NVIC_ST_CTRL_INTEN      0x00000002  // Interrupt enable
//#define NVIC_ST_CTRL_ENABLE     0x00000001  // Counter mode
//#define GPIO_PORTD_DIR_R        (*((volatile unsigned long *)0x40007400))
//#define GPIO_PORTD_DEN_R        (*((volatile unsigned long *)0x4000751C))
//#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
//#define SYSCTL_RCGC2_GPIOD      0x00000008  // port D Clock Gating Control

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
volatile unsigned long Counts = 0;
#define GPIO_PORTD0             (*((volatile unsigned long *)0x40007004))

// Initialize Systick periodic interrupts
// Units of period are 20ns
// Range is up to 2^24-1
void SysTick_Init(unsigned long period){
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOD; // activate port D
  Counts = 0;
  GPIO_PORTD_DIR_R |= 0x01;   // make PD0 out
  GPIO_PORTD_DEN_R |= 0x01;   // enable digital I/O on PD0
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_RELOAD_R = period - 1;// reload value
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x40000000; // priority 2
                              // enable SysTick with core clock and interrupts
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
  EnableInterrupts();
}

//debug code
// Executed every 1 ms
void SysTick_Handler(void){
  GPIO_PORTD0 ^= 0x01;        // toggle PD0
  Counts = Counts + 1;
}
int main(void){       // bus clock at 50 MHz
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_8MHZ);
  SysTick_Init(50000);     // initialize SysTick timer
  while(1){
    WaitForInterrupt();
  }
}
