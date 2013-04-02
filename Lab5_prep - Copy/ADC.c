
#include "ADC.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "inc/lm3s8962.h"
#include "debug.h"
#include "rit128x96x4.h"
#include <stdio.h>
#include <string.h>

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
// static int _ADC_EnableNVICInterrupt(int channelNum);
// static int _ADC_DisableNVICInterrupt(int channelNum);
// static int _ADC_SetNIVCPriority(int channelNum, unsigned int priority);
// static void _ADC_ADC0_Init(void);
// static void _ADC_ADC1_Init(void);
// static void _ADC_ADC2_Init(void);
// static void _ADC_ADC3_Init(void);

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
  
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN1;          // disable sample sequencer 1
  ADC_IM_R &= ~ADC_IM_MASK1;                // disable SS1 interrupts
  
  ADC_EMUX_R &= ~ADC_EMUX_EM1_M;            // clear SS1 trigger select field
  ADC_EMUX_R += ADC_EMUX_EM1_TIMER;         // configure for timer trigger event
  ADC_SSMUX1_R = 0;                         // clear
  ADC_SSMUX1_R += (3 << ADC_SSMUX1_MUX3_S) +
                  (2 << ADC_SSMUX1_MUX2_S) +
                  (1 << ADC_SSMUX1_MUX1_S) +
                  (0 << ADC_SSMUX1_MUX0_S); // sample ADC0 in mux 0, ADC1 in mux 1, ADC2 in m2, ADC3 in m3
  ADC_SSCTL1_R = 0x00006000;                // 4th sample is end of sequence, 4th sample int enable
  
  ADC_IM_R |= ADC_IM_MASK1;                // enable SS1 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN1;           // enable sample sequencer 1
  
  NVIC_EN0_R |= NVIC_EN0_INT15;           // enable NVIC int 15 (SS 1)
  NVIC_PRI3_R = (NVIC_PRI3_R&0x0FFFFFFF)|(ADC_NVIC_PRIORITY << 29); // bits 29-31
  
//   _ADC_SetNIVCPriority(0, ADC_NVIC_PRIORITY);
//   _ADC_EnableNVICInterrupt(0);
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
//   long sr;
//   sr = StartCritical();
//   switch (channelNum) {
//     case 0:
//       _ADC_ADC0_Init();
//       break;
//     case 1:
//       _ADC_ADC1_Init();
//       break;
//     case 2:
//       _ADC_ADC2_Init();
//       break;
//     case 3:
//       _ADC_ADC3_Init();
//       break;
//     default:
//       EndCritical(sr);
//       return 0;
//   }
//   EndCritical(sr);
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

void ADC1_Handler(void) {
  int i;
  unsigned short data;
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x02;
  #endif
  ADC_ISC_R |= ADC_ISC_IN1;             // acknowledge ADC sequence 1 completion
  for(i = 0; i < 4; i++) {
    data = ADC_SSFIFO1_R & ADC_SSFIFO1_DATA_M;
    ADCMailBox[i] = data;
    ADCHasData[i] = TRUE;
    if(_ADC_tasks[i] != NULL) {
      _ADC_tasks[i](data);
    }
  }
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x02;
  #endif
}


#define FILTER_LEN 51

/*extern int FilterEn;
extern int ScopeMode;
extern long FFT_in[64], FFT_out[64];
extern unsigned long X[FILTER_LEN], Y[FILTER_LEN];

int ADC_ChangeScopeMode(void)
{
	ScopeMode ^= 1;
// 	memset(FFT_in, 0, sizeof(long)*64);
// 	memset(FFT_out, 0, sizeof(long)*64);
	return 0;
}

int ADC_ToggleFilter(void)
{
	FilterEn ^= 1;
	memset(X, 0, FILTER_LEN * sizeof(unsigned long));
	memset(Y, 0, FILTER_LEN * sizeof(unsigned long));
	RIT128x96x4PlotReClear();
	return 0;
}*/
