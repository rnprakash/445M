#include "os.h"
#include "mac.h"
#include "lm3s8962.h"
#include <stdio.h>

void OS_EthernetListener(void);

unsigned char RcvMessage[MAXBUF];
unsigned long ulUser0, ulUser1;
unsigned long RcvCount, XmtCount;
unsigned char Me[6];
unsigned char All[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#pragma O0
int OS_EthernetInit(void) {
  volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOD; // activate port D
  delay = SYSCTL_RCGC2_R;
  
  RcvCount = 0;
  XmtCount = 0;
  
  GPIO_PORTD_DIR_R |= 0x01;   // make PD0 out
  GPIO_PORTD_DEN_R |= 0x01;   // enable digital I/O on PD0
  GPIO_PORTF_DIR_R &= ~0x02;  // make PF1 in (PF1 built-in select button)
  GPIO_PORTF_DIR_R |= 0x01;   // make PF0 out (PF0 built-in LED)
  GPIO_PORTF_DEN_R |= 0x03;   // enable digital I/O on PF1, PF0
  GPIO_PORTF_PUR_R |= 0x02;   // PF1 has pullup
  
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
  
  OS_AddThread(&OS_EthernetListener, 128, 4);
  
  return 0;
}

void OS_EthernetListener(void) {
  unsigned long size;
  while(1) {
    size = MAC_ReceiveNonBlocking(RcvMessage,MAXBUF);
    if(size){
      RcvCount++;
      printf("%d %s\n",size,RcvMessage+14);
    }
  }
}
