// Main.c
// Runs on LM3S8962
// Test of the low-level Ethernet interface
// uses a cross over cable to connect two LM3S8962 boards


/* This example accompanies the book
   Embedded Systems: Real-Time Operating Systems for the Arm Cortex-M3, Volume 3,  
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2012

   Program 9.1, section 9.3

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
Pin Left color      Left signal          Right color  Right signal
1   white/orange    TxData+     -----    white/green  TxData+
2   orange          TxData-     -----    green        TxData-
3   white/green     RecvData+   -----    white/orange RecvData+
4   blue            Unused      -----    blue         Unused
5   white/blue      Unused      -----    white/blue   Unused
6   green           RecvData-   -----    orange       RecvData-
7   white/brown     Unused      -----    white/brown  Unused
8   brown           Unused      -----    brown        Unused
Table 9.3. Pin assignments for a crossover Ethernet cable
*/
#include <stdio.h>
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "lm3s8962.h"
#include "MAC.h"
#include "Output.h"

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
volatile unsigned long XmtCount = 0;
volatile unsigned long RcvCount = 0;
#define GPIO_PD0  (*((volatile unsigned long *)0x40007004))
#define GPIO_PF0  (*((volatile unsigned long *)0x40025004))
#define GPIO_PF1  (*((volatile unsigned long *)0x40025008))

// Initialize Systick periodic interrupts
// Units of period are 20ns
// Range is up to 2^24-1
void SysTick_Init(unsigned long period){
  XmtCount = 0;
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_RELOAD_R = period - 1;// reload value
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x40000000; // priority 2
                              // enable SysTick with core clock and interrupts
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
  EnableInterrupts();
}
unsigned long ulUser0, ulUser1;
unsigned char Me[6];
unsigned char All[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
#define MAXBUF 200
unsigned char RcvMessage[MAXBUF];
unsigned char const XmtMessage1[52]= "Hello there";
unsigned char const XmtMessage2[52]= "Ethernet works";
unsigned char const XmtMessage3[52]= "Short packets";
unsigned char const XmtMessage4[52]= "FF-FF-FF-FF-FF-FF";
unsigned char const XmtMessage5[52]= "100BASE-TX";
unsigned long LastSwitch=0;
long XmtResult;
#define SIXTYSECONDS 100000000;
unsigned long ScreenSave;
void SysTick_Handler(void){      // Executed every 1 ms
unsigned char *pt;
  GPIO_PD0 ^= 0x01;              // toggle PD0
  if(LastSwitch&&(GPIO_PF1==0)){ // touch
    GPIO_PF0 ^= 0x01;            // toggle PF0
    XmtCount = XmtCount + 1;
    switch(XmtCount%5){
      case 0:  pt = (unsigned char *)XmtMessage1; break;
      case 1:  pt = (unsigned char *)XmtMessage2; break;
      case 2:  pt = (unsigned char *)XmtMessage3; break;
      case 3:  pt = (unsigned char *)XmtMessage4; break;
      default: pt = (unsigned char *)XmtMessage5; break;
    }
    XmtResult = MAC_SendData(pt,52,All);
    ScreenSave = SIXTYSECONDS;
    Output_On();
  }
  LastSwitch = GPIO_PF1;
}
int main(void){       // bus clock at 50 MHz
  long size; 
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_8MHZ);
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOD; // activate port D
  ScreenSave = SIXTYSECONDS;

  GPIO_PORTD_DIR_R |= 0x01;   // make PD0 out
  GPIO_PORTD_DEN_R |= 0x01;   // enable digital I/O on PD0
  GPIO_PORTF_DIR_R &= ~0x02;  // make PF1 in (PF1 built-in select button)
  GPIO_PORTF_DIR_R |= 0x01;   // make PF0 out (PF0 built-in LED)
  GPIO_PORTF_DEN_R |= 0x03;   // enable digital I/O on PF1, PF0
  GPIO_PORTF_PUR_R |= 0x02;   // PF1 has pullup

  Output_Init();
  Output_Color(8);
  printf("Low-level Ethernet.\r");

    // For the Ethernet Eval Kits, the MAC address will be stored in the
    // non-volatile USER0 and USER1 registers.  
  ulUser0 = FLASH_USERREG0_R;
  ulUser1 = FLASH_USERREG1_R;
  Me[0] = ((ulUser0 >>  0) & 0xff);
  Me[1] = ((ulUser0 >>  8) & 0xff);
  Me[2] = ((ulUser0 >> 16) & 0xff);
  Me[3] = ((ulUser1 >>  0) & 0xff);
  Me[4] = ((ulUser1 >>  8) & 0xff);
  Me[5] = ((ulUser1 >> 16) & 0xff);

  MAC_Init(Me);
  SysTick_Init(50000);     // initialize SysTick timer
  printf("Ethernet linked.\r");
  while(1){
    size = MAC_ReceiveNonBlocking(RcvMessage,MAXBUF);
    if(size){ 
      ScreenSave = SIXTYSECONDS;
      Output_On();
      RcvCount++;
      printf("%d %s\r",size,RcvMessage+14);
    }
    if(ScreenSave){
      ScreenSave--;
      if(ScreenSave==0){
        Output_Off();
      }
    }
  }
}
