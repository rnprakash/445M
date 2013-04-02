#include "OS.h"
#include "hw_types.h"
#include "lm3s8962.h"
#include <stdlib.h>

static void DebouncePortFTask(void);
static void DebouncePortETask(void);

static void(*_OS_SelTask)(void) = NULL;
static void(*_OS_DownTask)(void) = NULL;

//******** OS_AddButtonTask *************** 
// add a background task to run whenever the Select button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddButtonTask(void(*task)(void), unsigned long priority) {
  static unsigned int haveInit = 0;  // only initialize once
  if(!haveInit) {
    PORTF_Init();  // initialize; for now, just select switch (PF1)
    haveInit = 1;
  }
  _OS_SelTask = task;
  // initialize NVIC interrupts for port F
  NVIC_PRI7_R = ((NVIC_PRI7_R&0xFF0FFFFFFF)
									| (priority << 21));
  NVIC_EN0_R |= NVIC_EN0_INT30;
  return 1;  
}

// the same functionality as AddButtonTask, except for port E
int OS_AddDownTask(void(*task)(void), unsigned long priority) {
  static unsigned int haveInit = 0;  // only initialize once
  if(!haveInit) {
    PORTE_Init();  // initialize; for now, just down switch (PE1)
    haveInit = 1;
  }
  _OS_DownTask = task;
  // initialize NVIC interrupts for port E
  NVIC_PRI1_R = ((NVIC_PRI1_R&0xFFFFFFFF0F)
									| (priority << 5));
  NVIC_EN0_R |= NVIC_EN0_INT4;
  return 1;  
}

//static unsigned long LastPF1 = 1;
void GPIOPortF_Handler(void) {
  if(_OS_SelTask != NULL) {
    OS_AddThread(_OS_SelTask, _OS_STACK_SIZE, 1);
  }
  GPIO_PORTF_IM_R &= ~PORTF_PINS; // disarm interrupt
  OS_AddThread(&DebouncePortFTask, _OS_STACK_SIZE, 1); // TODO - handle priority
}

void GPIOPortE_Handler(void) {
  if(_OS_DownTask != NULL) {
    OS_AddThread(_OS_DownTask, _OS_STACK_SIZE, 1);
  }
  GPIO_PORTE_IM_R &= ~PORTE_PINS; // disarm interrupt
  OS_AddThread(&DebouncePortETask, _OS_STACK_SIZE, 1); // TODO - handle priority
}

static void DebouncePortFTask(void) {
  OS_Sleep(BUTTON_SLEEP_MS);   // foreground sleeping, must run within 50ms
  GPIO_PORTF_ICR_R |= PORTF_PINS;    // acknowledge interrupt
  GPIO_PORTF_IM_R |= PORTF_PINS;    // re-arm interrupt
  OS_Kill();
  OS_Delay(OS_ARBITRARY_DELAY);
}

static void DebouncePortETask(void) {
  OS_Sleep(BUTTON_SLEEP_MS);   // foreground sleeping, must run within 50ms
  GPIO_PORTE_ICR_R |= PORTE_PINS;    // acknowledge interrupt
  GPIO_PORTE_IM_R |= PORTE_PINS;    // re-arm interrupt
  OS_Kill();
  OS_Delay(OS_ARBITRARY_DELAY);
}

void PORTF_Init(void) {
  volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF;
  delay = SYSCTL_RCGC2_R;
  GPIO_PORTF_IM_R &= ~PORTF_PINS;  // interrupt mask register should be set to 0 for setup
  GPIO_PORTF_DIR_R &= ~PORTF_PINS; // input
  GPIO_PORTF_DEN_R |= PORTF_PINS;  // digital mode
  GPIO_PORTF_PUR_R |= PORTF_PINS;  // enable pull-up res
  GPIO_PORTF_IS_R &= ~PORTF_PINS;  // edge-sensitive
  GPIO_PORTF_IBE_R &= ~PORTF_PINS;  // interrupt both edges
  GPIO_PORTF_ICR_R = PORTF_PINS;   // clear flags
  GPIO_PORTF_IM_R |= PORTF_PINS;   // re-arm interrupt
}

void PORTE_Init(void) {
  volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE;
  delay = SYSCTL_RCGC2_R;
  GPIO_PORTE_IM_R &= ~PORTE_PINS;  // interrupt mask register should be set to 0 for setup
  GPIO_PORTE_DIR_R &= ~PORTE_PINS; // input
  GPIO_PORTE_DEN_R |= PORTE_PINS;  // digital mode
  GPIO_PORTE_PUR_R |= PORTE_PINS;  // enable pull-up res
  GPIO_PORTE_IS_R &= ~PORTE_PINS;  // edge-sensitive
  GPIO_PORTE_IBE_R &= ~PORTE_PINS;  // not interrupt both edges
//  GPIO_PORTE_IEV_R &= ~PORTE_PINS;   // rising edge triggered
  GPIO_PORTE_ICR_R = PORTE_PINS;   // clear flags
  GPIO_PORTE_IM_R |= PORTE_PINS;   // re-arm interrupt
}
