
#include "ADC.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "inc/lm3s8962.h"
#include "debug.h"
#include <stdlib.h>

#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

#define ADC_NVIC_PRIORITY 3
#define PRESCALE 49 // constant prescale value, 1 us ticks
#define TIMER_RATE 5000000

static void _ADC_SetTimer0APeriod(unsigned int fs);
static int _ADC_EnableNVICInterrupt(int channelNum);
static int _ADC_DisableNVICInterrupt(int channelNum);
static int _ADC_SetNIVCPriority(int channelNum, unsigned int priority);
static void _ADC_ADC0_Init(void);
static void _ADC_ADC1_Init(void);
static void _ADC_ADC2_Init(void);
static void _ADC_ADC3_Init(void);

long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts

// mailbox and flag for each ADC channel
int ADCHasData[4] = {FALSE, FALSE, FALSE, FALSE};
unsigned short ADCMailBox[4];
static void(*_ADC_tasks[4])(unsigned short) = {NULL};

void ADC_Init(unsigned int fs) {
  volatile unsigned long delay;
  DisableInterrupts();
  SYSCTL_RCGC0_R |= SYSCTL_RCGC0_ADC;       // activate ADC
  SYSCTL_RCGC0_R &= ~SYSCTL_RCGC0_ADCSPD_M; // clear ADC sample speed field
  SYSCTL_RCGC0_R += SYSCTL_RCGC0_ADCSPD500K;// configure for 500K ADC max sample rate
  delay = SYSCTL_RCGC0_R;                   // allow time to finish activating
  // map ADC0-3 handlers to port D pins 0-3 for profiling
  #if DEBUG == 1
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
    delay = SYSCTL_RCGC2_R;
    GPIO_PORTB_DIR_R |= 0x0F;             // make PB0-3 out
    GPIO_PORTB_DEN_R |= 0x0F;             // enable digital I/O on PB0-3
    GPIO_PORTB_DATA_R &= ~0x0F;           // clear PB0-3
  #endif
  ADC_TimerInit(fs);
//  EnableInterrupts();
}

// fs in kHz
void ADC_TimerInit(unsigned int fs) {
  volatile unsigned long delay;
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER0;    // activate timer0
  delay = SYSCTL_RCGC1_R;                   // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN;          // disable timer0A during setup
  TIMER0_CTL_R |= TIMER_CTL_TAOTE;          // enable timer0A trigger to ADC
  TIMER0_CFG_R = TIMER_CFG_16_BIT;          // configure for 16-bit timer mode
  TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;   // configure for periodic mode
  TIMER0_TAPR_R = PRESCALE;                 // prescale value for trigger
  _ADC_SetTimer0APeriod(fs);
  TIMER0_IMR_R &= ~TIMER_IMR_TATOIM;        // disable timeout (rollover) interrupt
  TIMER0_CTL_R |= TIMER_CTL_TAEN;           // enable timer0A 16-b, periodic, no interrupts
}

// fs in Hz
// rate is (clock period)*(prescale + 1)(period + 1)
static void _ADC_SetTimer0APeriod(unsigned int fs) {
  unsigned int period =  fs;// * 1000; // 1000 us per msTIMER_RATE / fs;
  TIMER0_TAILR_R = period;                  // start value for trigger
}


// should this take a channel number as the argument?
int ADC_Open(int channelNum) {
  long sr;
  sr = StartCritical();
  switch (channelNum) {
    case 0:
      _ADC_ADC0_Init();
      break;
    case 1:
      _ADC_ADC1_Init();
      break;
    case 2:
      _ADC_ADC2_Init();
      break;
    case 3:
      _ADC_ADC3_Init();
      break;
    default:
      EndCritical(sr);
      return 0;
  }
  EndCritical(sr);
  return 1;
}  

unsigned short ADC_In(unsigned int channelNum) {
  unsigned short data;
  long sr;
  while(!ADCHasData[channelNum]) {
    ;
  }
  sr = StartCritical();
  data = ADCMailBox[channelNum];
  ADCHasData[channelNum] = FALSE;
  EndCritical(sr);
  return data;
}

int ADC_Collect(unsigned int channelNum, unsigned int fs, void(*task)(unsigned short)) {
//  int i;
  _ADC_SetTimer0APeriod(fs);
  _ADC_tasks[channelNum] = task;
//   for(i = 0; i < numberOfSamples; i++) {
//     buffer[i] = ADC_In(channelNum);
//   }
  return 1;
}

static int _ADC_EnableNVICInterrupt(int channelNum) {
  switch (channelNum) {
    case 0:
      NVIC_EN0_R |= NVIC_EN0_INT14;             // enable interrupt 14 in NVIC (ADC SS 0)
      break;
    case 1:
      NVIC_EN0_R |= NVIC_EN0_INT15;             // enable interrupt 15 in NVIC (ADC SS 1)
      break;
    case 2:
      NVIC_EN0_R |= NVIC_EN0_INT16;             // enable interrupt 16 in NVIC (ADC SS 2)
      break;
    case 3:
      NVIC_EN0_R |= NVIC_EN0_INT17;             // enable interrupt 17 in NVIC (ADC SS 3)
      break;
    default:
      return 0;
  }
  return 1;
}

static int _ADC_DisableNVICInterrupt(int channelNum) {
  // requires privelege mode
  switch (channelNum) {
    case 0:
      NVIC_DIS0_R |= NVIC_EN0_INT14;             // disable interrupt 14 in NVIC (ADC SS 0)
      break;
    case 1:
      NVIC_DIS0_R |= NVIC_EN0_INT15;             // disable interrupt 15 in NVIC (ADC SS 1)
      break;
    case 2:
      NVIC_DIS0_R |= NVIC_EN0_INT16;             // disable interrupt 16 in NVIC (ADC SS 2)
      break;
    case 3:
      NVIC_DIS0_R |= NVIC_EN0_INT17;             // disable interrupt 17 in NVIC (ADC SS 3)
      break;
    default:
      return 0;
  }
  return 1;
}

static int _ADC_SetNIVCPriority(int channelNum, unsigned int priority) {
  if (priority > 7) {
    return 0;
  }
  switch (channelNum) {
    case 0:
      NVIC_PRI3_R = (NVIC_PRI3_R&0xFF1FFFFF)|(priority << 21); // bits 21-23
      break;
    case 1:
      NVIC_PRI3_R = (NVIC_PRI4_R&0x1FFFFFFF)|(priority << 29); // bits 29-31
      break;
    case 2:
      NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFFFF1F)|(priority << 5); // bits 5-7
      break;
    case 3:
      NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF1FFF)|(priority << 13); // bits 13-15
      break;
    default:
      return 0;
  }
  return 1;
}

// default priority 0 (highest)
static void _ADC_ADC0_Init(void) {
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN0;          // disable sample sequencer 0
  ADC_EMUX_R &= ~ADC_EMUX_EM0_M;            // clear SS0 trigger select field
  ADC_EMUX_R += ADC_EMUX_EM0_TIMER;         // configure for timer trigger event
  ADC_SSMUX0_R &= ~ADC_SSMUX0_MUX0_M;      // clear SS0 1st sample input select field
                                            // configure for ADC0 as first sample input
  ADC_SSMUX0_R += (0 << ADC_SSMUX0_MUX3_S);
  ADC_SSCTL0_R = (/*0                        // settings for 1st sample:
                   & */~ADC_SSCTL0_TS3        // read pin specified by ADC0_SSMUX0_R
                   | ADC_SSCTL0_IE3         // raw interrupt asserted here
                   | ADC_SSCTL0_END3        // sample is end of sequence
                   & ~ADC_SSCTL0_D3);       // differential mode not used
  
  ADC_IM_R |= ADC_IM_MASK0;                // enable SS0 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN0;           // enable sample sequencer 0
  _ADC_SetNIVCPriority(0, ADC_NVIC_PRIORITY);
  _ADC_EnableNVICInterrupt(0);
}

// default priority 1
static void _ADC_ADC1_Init(void) {
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN1;          // disable sample sequencer 1
  ADC_EMUX_R &= ~ADC_EMUX_EM1_M;           // clear SS1 trigger select field
  ADC_EMUX_R += ADC_EMUX_EM1_TIMER;        // configure for timer trigger event
  ADC_SSMUX1_R &= ~ADC_SSMUX1_MUX0_M;      // clear SS1 1st sample input select field
                                            // configure for ADC1 as first sample input
  ADC_SSMUX1_R += (1 << ADC_SSMUX1_MUX0_S);
  ADC_SSCTL1_R = (/*0                        // settings for 1st sample:
                   &*/ ~ADC_SSCTL1_TS0        // read pin specified by ADC1_SSMUX0_R
                   | ADC_SSCTL1_IE0         // raw interrupt asserted here
                   | ADC_SSCTL1_END0        // sample is end of sequence
                   & ~ADC_SSCTL1_D0);       // differential mode not used
  
  ADC_IM_R |= ADC_IM_MASK1;                // enable SS1 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN1;           // enable sample sequencer 1
  _ADC_SetNIVCPriority(1, ADC_NVIC_PRIORITY);
  _ADC_EnableNVICInterrupt(1);
}

// default priority 2
static void _ADC_ADC2_Init(void) {
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN2;          // disable sample sequencer 2
  ADC_EMUX_R &= ~ADC_EMUX_EM2_M;           // clear SS2 trigger select field
  ADC_EMUX_R += ADC_EMUX_EM2_TIMER;        // configure for timer trigger event
  ADC_SSMUX2_R &= ~ADC_SSMUX2_MUX0_M;      // clear SS2 1st sample input select field
                                            // configure for ADC2 as first sample input
  ADC_SSMUX2_R += (2 << ADC_SSMUX2_MUX0_S);
  ADC_SSCTL2_R = (/*0                        // settings for 1st sample:
                   & */~ADC_SSCTL2_TS0        // read pin specified by ADC2_SSMUX0_R
                   | ADC_SSCTL2_IE0         // raw interrupt asserted here
                   | ADC_SSCTL2_END0        // sample is end of sequence
                   & ~ADC_SSCTL2_D0);       // differential mode not used
  
  ADC_IM_R |= ADC_IM_MASK2;                // enable SS2 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN2;           // enable sample sequencer 2
  _ADC_SetNIVCPriority(2, ADC_NVIC_PRIORITY);
  _ADC_EnableNVICInterrupt(2);
}

// default priority 3 (lowest)
static void _ADC_ADC3_Init(void) {
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN3;          // disable sample sequencer 3
  ADC_EMUX_R &= ~ADC_EMUX_EM3_M;           // clear SS3 trigger select field
  ADC_EMUX_R += ADC_EMUX_EM3_TIMER;        // configure for timer trigger event
  ADC_SSMUX3_R &= ~ADC_SSMUX3_MUX0_M;      // clear SS3 1st sample input select field
                                            // configure for ADC3 as first sample input
  ADC_SSMUX3_R += (3 /*<< ADC_SSMUX3_MUX0_S*/);
  ADC_SSCTL3_R = (/*0                        // settings for 1st sample:
                   &*/ ~ADC_SSCTL3_TS0        // read pin specified by ADC0_SSMUX3_R
                   | ADC_SSCTL3_IE0         // raw interrupt asserted here
                   | ADC_SSCTL3_END0        // sample is end of sequence (default setting, hardwired)
                   & ~ADC_SSCTL3_D0);       // differential mode not used
  
  ADC_IM_R |= ADC_IM_MASK3;                // enable SS3 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN3;           // enable sample sequencer 3
  _ADC_SetNIVCPriority(3, ADC_NVIC_PRIORITY);
  _ADC_EnableNVICInterrupt(3);
}

void ADC0_Handler(void) {
  long sr = StartCritical();
  unsigned short data;
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x01;
  #endif
  ADC_ISC_R |= ADC_ISC_IN0;             // acknowledge ADC sequence 0 completion
  data = ADC_SSFIFO0_R & ADC_SSFIFO0_DATA_M;
  ADCMailBox[0] = data;
  ADCHasData[0] = TRUE;
  if(_ADC_tasks[0] != NULL) {
    _ADC_tasks[0](data);
  }
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x01;
  #endif
  EndCritical(sr);
}

void ADC1_Handler(void) {
  long sr = StartCritical();
  unsigned short data;
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x02;
  #endif
  ADC_ISC_R |= ADC_ISC_IN1;             // acknowledge ADC sequence 1 completion
  data = ADC_SSFIFO1_R & ADC_SSFIFO1_DATA_M;
  ADCMailBox[1] = data;
  ADCHasData[1] = TRUE;
  if(_ADC_tasks[1] != NULL) {
    _ADC_tasks[1](data);
  }
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x02;
  #endif
  EndCritical(sr);
}

void ADC2_Handler(void) {
  long sr = StartCritical();
  unsigned short data;
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x04;
  #endif
  ADC_ISC_R |= ADC_ISC_IN2;             // acknowledge ADC sequence 2 completion
  data = ADC_SSFIFO2_R & ADC_SSFIFO2_DATA_M;
  ADCMailBox[2] = data;
  ADCHasData[2] = TRUE;
  if(_ADC_tasks[2] != NULL) {
    _ADC_tasks[2](data);
  }
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x04;
  #endif
  EndCritical(sr);
}

void ADC3_Handler(void) {
  long sr = StartCritical();
  unsigned short data;
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x08;
  #endif
  ADC_ISC_R |= ADC_ISC_IN3;             // acknowledge ADC sequence 3 completion
  data = ADC_SSFIFO3_R & ADC_SSFIFO2_DATA_M;
  ADCMailBox[3] = data;
  ADCHasData[3] = TRUE;
  if(_ADC_tasks[3] != NULL) {
    _ADC_tasks[3](data);
  }
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x08;
  #endif
  EndCritical(sr);
}
