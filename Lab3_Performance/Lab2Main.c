#include <stdio.h>
#include "hw_types.h"
#include "sysctl.h"
#include "rit128x96x4.h"
#include "OS.h"
#include "UART.h"
#include "shell.h"
#include "ADC.h"
#include "lm3s8962.h"

#define PROFILING 1
#if PROFILING == 1
  #define PINS 0x07
#endif

#define TIMESLICE               TIME_2MS    // thread switch time in system time units
#define RUNLENGTH 10000   // display results and quit when NumSamples==RUNLENGTH

void dummyTask1(void);
void dummyTask2(void);
void dummyTask3(void);
void dummyButtonTask(void);
void dummyDownTask(void);
void dummyPeriodicTask(void);
void jerkTask(void);
void PID(void);
void DAS(void);
void ButtonPush(void);
void Consumer(void);
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);
short PID_stm32(short Error, short *Coeff);

unsigned long NumCreated;   // number of foreground threads created
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long NumSamples;   // incremented every sample
unsigned long FilterWork;   // number of digital filter calculations finished
long MaxJitter;             // largest time jitter between interrupts in usec
long MinJitter;             // smallest time jitter between interrupts in usec
#define JITTERSIZE 64
unsigned long const JitterSize=JITTERSIZE;
unsigned long JitterHistogram[JITTERSIZE]={0,};
long x[64],y[64];         // input and output arrays for FFT

int main(void)
{
  #if PROFILING == 1
    volatile unsigned long delay;
  #endif
	/* Initialize 8MHz clock */
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  OLED_Init(15);
  ADC_Init(1000);
  ADC_Open(0);
  ADC_Open(1);
  SH_Init();
  OS_Init();
  OS_MailBox_Init();
  SH_Init();
  
  //********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(32);
  
  NumCreated = 0;
  NumSamples = 0;
  MaxJitter = 0;       // OS_Time in 20ns units
  MinJitter = 10000000;
  
  #if PROFILING
    // intialize port b pins as specified by PINS mask
    // for digital output for use in profiling threads
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
    delay = SYSCTL_RCGC2_R;
    GPIO_PORTB_DIR_R |= PINS;
    GPIO_PORTB_DEN_R |= PINS;
    GPIO_PORTB_DATA_R &= ~PINS;
  #endif
  
  // testing/debugging stuff
  OS_Add_Periodic_Thread(&DAS,2,1);
//  OS_AddButtonTask(&dummyButtonTask, 1);
  OS_AddButtonTask(&ButtonPush, 1);
  OS_AddDownTask(&ButtonPush, 1);
  
//   NumCreated += OS_AddThread(&jerkTask, 0, 6);
//   NumCreated += OS_AddThread(&dummyTask3, 0, 7);
//   NumCreated += OS_AddThread(&dummyTask1, 0, 2);
  NumCreated += OS_AddThread(&PID, 128, 5);
//   NumCreated += OS_AddThread(&dummyTask2, 0, 2);
  NumCreated += OS_AddThread(&Consumer, 128, 0);
  NumCreated += OS_AddThread(&SH_Shell, 128, 6);
  OS_Launch(TIMESLICE);
	
	/* Loop indefinitely */
  while(1);
}

OS_SemaphoreType binarySemaphore;
//int count1 = 0;
void dummyTask1(void) {
//   int i;
//   #if PROFILING == 1
//     int myPin = 0x01;
//   #endif
  OS_InitSemaphore(&binarySemaphore, OS_BINARY_SEMAPHORE);
  while(1) {
    OS_bWait(&binarySemaphore);
    OLED_Out(TOP, "task 1 acquired");
//     for(i = 0; i < 100000; i++) {
//       #if PROFILING == 1
//         GPIO_PORTB_DATA_R ^= myPin;
//       #endif
//     }
    OS_Sleep(1000);
    OS_bSignal(&binarySemaphore);
    OLED_Out(TOP, "task 1 released");
    OS_Sleep(1000);
//     for(i = 0; i < 100000; i++) {
//       #if PROFILING == 1
//         GPIO_PORTB_DATA_R ^= myPin;
//       #endif
//     }
//     OLED_Out(TOP, "task 1 dead");
//     OS_AddThread(&dummyTask2, 0 ,1);
//     OS_Kill();
  }
}

//int count2 = 0;
void dummyTask2(void) {
//   int i;
//   unsigned long data;
//   #if PROFILING == 1
//     int myPin = 0x02;
//   #endif
  while(1) {
 //     OLED_Out(BOTTOM, "task 2 request");
      OS_bWait(&binarySemaphore);
      OLED_Out(BOTTOM, "task 2 acquired");
//     for(i = 0; i < 100000; i++) {
//       #if PROFILING == 1
//         GPIO_PORTB_DATA_R ^= myPin;
//       #endif
//     }
    OS_Sleep(1000);
    OS_bSignal(&binarySemaphore);
    OLED_Out(BOTTOM, "task 2 released");
    OS_Sleep(1000);
//     for(i = 0; i < 100000; i++) {
//       #if PROFILING == 1
//         GPIO_PORTB_DATA_R ^= myPin;
//       #endif
//     }
//    data = OS_MailBox_Recv();
//    OLED_Out(BOTTOM, "task 2 receive");
    
//     OS_AddThread(&dummyTask1, 0, 1);
//     OLED_Out(BOTTOM, "task 2 dead");
//     OS_Kill();
//    OS_Sleep(2000);
  }
}

int count3 = 0;
void dummyTask3(void) {
  count3++;
  OS_Sleep(4000); // sleep for 4 seconds
  OLED_Out(BOTTOM, "task 3 Yaaaaaaaaaawn");
  NumCreated += OS_AddThread(&dummyTask3, 64, 2);
  OS_Kill();
//   #if PROFILING == 1
//     int myPin = 0x04;
//   #endif
//   while(1) {
//     count3++;
//     #if PROFILING == 1
//       GPIO_PORTB_DATA_R ^= myPin;
//     #endif
//   }
}

void dummyButtonTask(void) {
  OLED_Out(TOP, "Select Pressed");
  OS_Kill();
}

void dummyDownTask(void) {
  OLED_Out(BOTTOM, "Down Pressed");
  OS_Kill();
}

unsigned long countPeriodic = 0;
void dummyPeriodicTask(void) {
  countPeriodic++;
}

// never sleeps or blocks
void jerkTask(void) {
  while(1)
    ;
}

//------------------Task 1--------------------------------
// 2 kHz sampling ADC channel 1, using software start trigger
// background thread executed at 2 kHz
// 60-Hz notch IIR filter, assuming fs=2000 Hz
// y(n) = (256x(n) -503x(n-1) + 256x(n-2) + 498y(n-1)-251y(n-2))/256
short Filter(short data){
static short x[6]; // this MACQ needs twice
static short y[6];
static unsigned int n=3;   // 3, 4, or 5
  n++;
  if(n==6) n=3;     
  x[n] = x[n-3] = data;  // two copies of new data
  y[n] = (256*(x[n]+x[n-2])-503*x[1]+498*y[1]-251*y[n-2]+128)/256;
  y[n-3] = y[n];         // two copies of filter outputs too
  return y[n];
} 
//******** DAS *************** 
// background thread, calculates 60Hz notch filter
// runs 2000 times/sec
// inputs:  none
// outputs: none
unsigned short DASoutput;

void DAS(void){
	int index;
	unsigned short input;  
	unsigned static long LastTime;  // time at previous ADC sample
	unsigned long thisTime;         // time at current ADC sample
	long jitter;                    // time between measured and expected
  if(NumSamples < RUNLENGTH){   // finite time run
    input = ADC_In(1);
    thisTime = OS_Time();       // current time, 20 ns
    DASoutput = Filter(input);
    FilterWork++;        // calculation finished
    if(FilterWork>1){    // ignore timing of first interrupt
      jitter = OS_TimeDifference(LastTime,thisTime) - 100000/50;  // in usec
      if(jitter > MaxJitter){
        MaxJitter = jitter;
      }
      if(jitter < MinJitter){
        MinJitter = jitter;
      }        // jitter should be 0
      index = jitter+JITTERSIZE/2;   // us units
      if(index<0)index = 0;
      if(index>=JitterSize) index = JITTERSIZE-1;
      JitterHistogram[index]++;
    }
    LastTime = thisTime;
  }
}
//--------------end of Task 1-----------------------------

//------------------Task 2--------------------------------
// background thread executes with select button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************
void ButtonWork(void){
unsigned long i;
unsigned long myId = OS_Id(); 
  char str[20];
  sprintf(str, "NumCreated = %d", NumCreated);
  OLED_Out(BOTTOM, str); 
  if(NumSamples < RUNLENGTH){   // finite time run
    for(i=0;i<20;i++){  // runs for 2 seconds
      OS_Sleep(50);     // set this to sleep for 0.1 sec
    }
  }
  sprintf(str, "PIDWork    = %d", PIDWork);
  OLED_Out(BOTTOM, str);
  sprintf(str, "DataLost   = %d", DataLost);
  OLED_Out(BOTTOM, str);
  sprintf(str, "Jitter(us) = %d",MaxJitter-MinJitter);
  OLED_Out(BOTTOM, str);
  OLED_Out(BOTTOM, "");
  OS_Kill();  // done
  OS_Delay(OS_ARBITRARY_DELAY);
} 

//************ButtonPush*************
// Called when Select Button pushed
// Adds another foreground task
// background threads execute once and return
void ButtonPush(void){
  if(OS_AddThread(&ButtonWork,100,1)){
    NumCreated++; 
  }
  OS_Kill();
  OS_Delay(OS_ARBITRARY_DELAY);
}

//------------------Task 3--------------------------------
// hardware timer-triggered ADC sampling at 1 kHz
// Producer runs as part of ADC ISR
// Producer uses fifo to transmit 1000 samples/sec to Consumer
// every 64 samples, Consumer calculates FFT
// every 64 ms, consumer sends data to Display via mailbox
// Display thread updates oLED with measurement

//******** Producer *************** 
// The Producer in this lab will be called from your ADC ISR
// A timer runs at 1 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 1 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the consumer, runs periodically at 1 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){
  if(NumSamples < RUNLENGTH){   // finite time run
    NumSamples++;               // number of samples
    if(OS_Fifo_Put(data) == 0){ // send to consumer
      DataLost++;
    } 
  } 
}
void Display(void); 

//******** Consumer *************** 
// foreground thread, accepts data from producer
// calculates FFT, sends DC component to Display
// inputs:  none
// outputs: none
void Consumer(void){
unsigned long data,DCcomponent; // 10-bit raw ADC sample, 0 to 1023
unsigned long t;  // time in ms
unsigned long myId = OS_Id();
  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz
  NumCreated += OS_AddThread(&Display,128,0); 
  while(NumSamples < RUNLENGTH) {
    for(t = 0; t < 64; t++){   // collect 64 ADC samples
      data = OS_Fifo_Get();    // get from producer
      x[t] = data;             // real part is 0 to 1023, imaginary part is 0
    }
    cr4_fft_64_stm32(y,x,64);  // complex FFT of last 64 ADC values
    DCcomponent = y[0]&0xFFFF; // Real part at frequency 0, imaginary part should be zero
     OS_MailBox_Send(DCcomponent);
  }
  OLED_Out(BOTTOM, "CONSUMER DONE");
  OS_Kill();  // done
  OS_Delay(OS_ARBITRARY_DELAY);
}

static unsigned long voltage;
void DisplayThread(void)
{
	char str[20];
	while(NumSamples < RUNLENGTH)
	{
		sprintf(str, "Time left is %d", (RUNLENGTH-NumSamples)/1000);
		_OLED_Message(TOP, 1, str, 15);
		sprintf(str, "v(mV) = %d", voltage);
		_OLED_Message(TOP, 2, str, 15);
//    OS_Suspend();
     OS_Sleep(500);
	}
	OS_Kill();
	OS_Delay(OS_ARBITRARY_DELAY);
}

//******** Display *************** 
// foreground thread, accepts data from consumer
// displays calculated results on the LCD
// inputs:  none                            
// outputs: none
void Display(void){ 
unsigned long data;
  char str[20];
   sprintf(str, "Run length is %d", RUNLENGTH/1000);
  OLED_Out(TOP, str);   // top half used for Display
  NumCreated += OS_AddThread(&DisplayThread, 128, 5);
  while(NumSamples < RUNLENGTH) {
//    sprintf(str, "Time left is %d", (RUNLENGTH-NumSamples)/1000);
//     OS_LogEvent(EVENT_OLED_START);
//    OLED_Out(TOP, str);   // top half used for Display
//     OS_LogEvent(EVENT_OLED_FINISH);
    data = OS_MailBox_Recv();
    voltage = 3000*data/1024;               // calibrate your device so voltage is in mV
//		sprintf(str, "v(mV) = %d", voltage);
//		OLED_Out(TOP, str);
// 		OS_Delay(OS_ARBITRARY_DELAY);
  }
	OLED_Out(BOTTOM, "DONE");
	OS_Kill();  // done
  OS_Delay(OS_ARBITRARY_DELAY);
}

//--------------end of Task 3-----------------------------

//------------------Task 4--------------------------------
// foreground thread that runs without waiting or sleeping
// it executes a digital controller 
//******** PID *************** 
// foreground thread, runs a PID controller
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
short IntTerm;     // accumulated error, RPM-sec
short PrevError;   // previous error, RPM
short Coeff[3];    // PID coefficients
short Actuator;
void PID(void){ 
  short err;  // speed error, range -100 to 100 RPM
  unsigned long myId = OS_Id();
  PIDWork = 0;
  IntTerm = 0;
  PrevError = 0;
  Coeff[0] = 384;   // 1.5 = 384/256 proportional coefficient
  Coeff[1] = 128;   // 0.5 = 128/256 integral coefficient
  Coeff[2] = 64;    // 0.25 = 64/256 derivative coefficient*
  while(NumSamples < RUNLENGTH) {
    for(err = -1000; err <= 1000; err++){    // made-up data
      Actuator = PID_stm32(err,Coeff)/256;
    }
    PIDWork++;        // calculation finished
  }
  for(;;){;}          // done
}
//--------------end of Task 4-----------------------------
