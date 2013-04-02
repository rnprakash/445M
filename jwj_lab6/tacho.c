// LongPeriodMeasure.c
// Runs on LM3S811
// Use Timer0A in edge time mode to request interrupts on the rising
// edge of PD4 (CCP0).  In Timer0B periodic interrupts, count amount
// of time between rising edges to determine period.
// Daniel Valvano
// June 27, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011

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

// external signal connected to PD4 (CCP0) (trigger on rising edge)
// debugging heartbeat connected to PC5 (high while in interrupt)

#define NVIC_EN0_INT20          0x00100000  // Interrupt 20 enable
#define NVIC_EN0_INT19          0x00080000  // Interrupt 19 enable
#define NVIC_EN0_R              (*((volatile unsigned long *)0xE000E100))  // IRQ 0 to 31 Set Enable Register
#define NVIC_PRI4_R             (*((volatile unsigned long *)0xE000E410))  // IRQ 16 to 19 Priority Register
#define NVIC_PRI5_R             (*((volatile unsigned long *)0xE000E414))  // IRQ 20 to 23 Priority Register
#define TIMER0_CFG_R            (*((volatile unsigned long *)0x40030000))
#define TIMER0_TAMR_R           (*((volatile unsigned long *)0x40030004))
#define TIMER0_TBMR_R           (*((volatile unsigned long *)0x40030008))
#define TIMER0_CTL_R            (*((volatile unsigned long *)0x4003000C))
#define TIMER0_IMR_R            (*((volatile unsigned long *)0x40030018))
#define TIMER0_ICR_R            (*((volatile unsigned long *)0x40030024))
#define TIMER0_TAILR_R          (*((volatile unsigned long *)0x40030028))
#define TIMER0_TBILR_R          (*((volatile unsigned long *)0x4003002C))
#define TIMER_CFG_16_BIT        0x00000004  // 16-bit timer configuration,
                                            // function is controlled by bits
                                            // 1:0 of GPTMTAMR and GPTMTBMR
#define TIMER_TAMR_TACMR        0x00000004  // GPTM TimerA Capture Mode
#define TIMER_TAMR_TAMR_CAP     0x00000003  // Capture mode
#define TIMER_TBMR_TBMR_PERIOD  0x00000002  // Periodic Timer mode
#define TIMER_CTL_TAEN          0x00000001  // GPTM TimerA Enable
#define TIMER_CTL_TBEN          0x00000100  // GPTM TimerB Enable
#define TIMER_CTL_TAEVENT_M     0x0000000C  // GPTM TimerA Event Mask
#define TIMER_CTL_TAEVENT_POS   0x00000000  // Positive edge
#define TIMER_IMR_CAEIM         0x00000004  // GPTM CaptureA Event Interrupt
                                            // Mask
#define TIMER_IMR_TBTOIM        0x00000100  // GPTM TimerB Time-Out Interrupt
                                            // Mask
#define TIMER_ICR_CAECINT       0x00000004  // GPTM CaptureA Event Interrupt
                                            // Clear
#define TIMER_ICR_TBTOCINT      0x00000100  // GPTM TimerB Time-Out Interrupt
                                            // Clear
#define TIMER_TAILR_TAILRL_M    0x0000FFFF  // GPTM TimerA Interval Load
                                            // Register Low
#define TIMER_TBILR_TBILRL_M    0x0000FFFF  // GPTM TimerB Interval Load
                                            // Register
#define GPIO_PORTC_DIR_R        (*((volatile unsigned long *)0x40006400))
#define GPIO_PORTC_DEN_R        (*((volatile unsigned long *)0x4000651C))
#define GPIO_PORTC5             (*((volatile unsigned long *)0x40006080))
#define GPIO_PORTD_AFSEL_R      (*((volatile unsigned long *)0x40007420))
#define SYSCTL_RCGC1_R          (*((volatile unsigned long *)0x400FE104))
#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
#define SYSCTL_RCGC1_TIMER0     0x00010000  // timer 0 Clock Gating Control
#define SYSCTL_RCGC2_GPIOD      0x00000008  // port D Clock Gating Control
#define SYSCTL_RCGC2_GPIOC      0x00000004  // port C Clock Gating Control

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

unsigned short Period;  /* Period in msec */
unsigned char OverFlow; /* Set if Period is too big*/
unsigned char Done;     /* Set each rising edge of Timer0A */
unsigned short Cnt;     /* number of msec in one period */
void PWMeasure3_Init(void){
  // **** general initialization ****
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER0;// activate timer0
	                                      // activate timer3
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER3;
                                        // activate port A for CCP 1 (input capture for timer0B)
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA;
  Period = 0;                      // allow time to finish activating
  OverFlow = 0;
  Done = 0;
  Cnt = 0;
  GPIO_PORTA_DEN_R |= 0x40;        // enable digital I/O on PA6
  GPIO_PORTD_AFSEL_R |= 0x40;      // enable alt funct on PA6
                                   // disable timer0B during setup
  TIMER0_CTL_R &= ~(TIMER_CTL_TBEN);
  TIMER0_CFG_R = TIMER_CFG_16_BIT; // configure for 16-bit timer mode
  // **** timer0A initialization ****
                                   // configure for capture mode
  TIMER3_TAMR_R = (TIMER_TAMR_TACMR|TIMER_TAMR_TAMR_CAP);
                                   // clear trigger event field
  TIMER3_CTL_R &= ~TIMER_CTL_TAEVENT_M;
                                   // configure for rising edge event
  TIMER3_CTL_R += TIMER_CTL_TAEVENT_POS;
  TIMER3_TAILR_R = ;// maximum start value
  TIMER3_IMR_R |= TIMER_IMR_TATOIM; // enable rollover match interrupt
  TIMER3_ICR_R = TIMER_ICR_CAECINT;// clear timer0A capture match flag
  // **** timer0B initialization ****
                                   // configure for periodic mode
  TIMER0_TBMR_R = TIMER_TBMR_TBMR_PERIOD;
  TIMER0_TBILR_R = 6000;           // start value for 1000 Hz interrupts
  TIMER0_IMR_R |= TIMER_IMR_TBTOIM;// enable timeout (rollover) interrupt
  TIMER0_ICR_R = TIMER_ICR_TBTOCINT;// clear timer0B timeout flag
  TIMER0_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 16-b, +edge, interrupts
  // **** interrupt initialization ****
                                   // Timer0A=priority 2
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000; // top 3 bits
                                   // Timer0B=priority 2
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFFFF00)|0x00000040; // bits 5-7
                                   // enable interrupts 19 and 20 in NVIC
  NVIC_EN0_R |= NVIC_EN0_INT19+NVIC_EN0_INT20;
  EnableInterrupts();
}
// Interrupt on rising edge of PD4 (CCP0)
void Timer0A_Handler(void){
  TIMER0_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
  if(OverFlow){
    Period = 65535;                // actual period may be greater
    OverFlow = 0;
  }
  else
    Period = Cnt;
  Cnt = 0;
  Done = 0xFF;
  // restart Timer0B
  TIMER0_CTL_R &= ~TIMER_CTL_TBEN; // disable timer0B during configuration
  TIMER0_TBILR_R = 6000;           // start value for 1000 Hz interrupts
  TIMER0_ICR_R = TIMER_ICR_TBTOCINT;// clear timer0B timeout flag
  TIMER0_CTL_R |= TIMER_CTL_TBEN;  // enable timer0B 16-b, periodic, interrupts
}
// Interrupt every 1 ms after rising edge of PD4 (CCP0)
void Timer0B_Handler(void){
  GPIO_PORTC5 = 0x20;              // heartbeat
  TIMER0_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge timer0B timeout
  Cnt = Cnt + 1;
  if(Cnt==0) OverFlow=0xFF;
  GPIO_PORTC5 = 0x00;
}

//debug code
int main(void){
  PWMeasure3_Init();               // initialize timer0
  while(1){
    WaitForInterrupt();
  }
}
