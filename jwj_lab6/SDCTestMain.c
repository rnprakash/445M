// SDCTestMain.c
// Runs on LM3S8962
// Test Secure Digital Card read/write interface by writing test
// data, reading them back, and verifying that they match as
// expected.  Running the unformatted file tests will destroy
// any formatting already on the disk.  The formatted file tests
// will not work unless the disk has already been formatted.
// Valvano
// August 12, 2011

/* This example accompanies the book
   Embedded Systems: Real-Time Operating Systems for the Arm Cortex-M3, Volume 3,  
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2012

   Volume 3, Program 6.3, section 6.6

 Copyright 2012 by Jonathan W. Valvano, valvano@mail.utexas.edu
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
/*
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "inc/lm3s8962.h"
#include "edisk.h"

#define GPIO_PORTF2             (*((volatile unsigned long *)0x40025010))

void disk_timerproc (void); // function in edisk.c to be called every 10 ms
void SysTick_Init(unsigned long period); // Initialize SysTick periodic interrupts
// Executed every 10 ms
void SysTick_Handler(void){
  disk_timerproc();
}

unsigned char buffer[512];
#define MAXBLOCKS 100
void diskError(char* errtype, unsigned long n){
  GPIO_PORTF2 = 0x00;      // turn LED off to indicate error
  while(1){};
}
void TestDisk(void){  DSTATUS result;  unsigned short block;  int i; unsigned long n;
  // simple test of eDisk
  result = eDisk_Init(0);  // initialize disk
  if(result) diskError("eDisk_Init",result);
  n = 1;    // seed
  for(block = 0; block < MAXBLOCKS; block++){
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      buffer[i] = 0xFF&n;
    }
    if(eDisk_WriteBlock(buffer,block))diskError("eDisk_WriteBlock",block); // save to disk
  }
  n = 1;  // reseed, start over to get the same sequence
  for(block = 0; block < MAXBLOCKS; block++){
    if(eDisk_ReadBlock(buffer,block))diskError("eDisk_ReadBlock",block); // read from disk
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      if(buffer[i] != (0xFF&n)){
          GPIO_PORTF2 = 0x00;   // turn LED off to indicate error
      }
    }
  }
}


int main(void){ int i=0; 
                              // bus clock at 50 MHz
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_8MHZ);
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  SysTick_Init(500000);       // initialize SysTick timer, 10ms
  GPIO_PORTF_DIR_R |= 0x04;   // make PF2 out (built-in LED)
  GPIO_PORTF_DEN_R |= 0x04;   // enable digital I/O on PF2
  GPIO_PORTF2 = 0x04;         // turn LED on
// *******************unformatted file tests********************
  while(i<3){
    TestDisk();
    i = i + 1;
  }
  while(1){};
// ****************end of unformatted file tests****************
}
*/