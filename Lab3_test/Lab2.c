//*****************************************************************************
//
// Lab2.c - user programs
// Jonathan Valvano, Feb 4, 2011, EE345M
//
//*****************************************************************************
// feel free to adjust these includes as needed
#include <stdio.h>
#include <string.h>
#include "inc/hw_types.h"
#include "sysctl.h"
#include "lm3s8962.h"
//#include "serial.h"
#include "rit128x96x4.h"
#include "ADC.h"
#include "os.h"
#include "shell.h"
#include "uart.h"

//#include "profiling.h"
//#if PROFILING == 1
 // #define PINS 0x07
//#endif

//#define  OLED_Out

unsigned long NumCreated;   // number of foreground threads created
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
long MaxJitter;             // largest time jitter between interrupts in usec
long MinJitter;             // smallest time jitter between interrupts in usec
#define JITTERSIZE 64
unsigned long const JitterSize=JITTERSIZE;
unsigned long JitterHistogram[JITTERSIZE]={0,};

#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units
#define PERIOD TIME_1MS/2     // 2kHz sampling period in system time units
// 10-sec finite time experiment duration 
#define RUNLENGTH 10000   // display results and quit when NumSamples==RUNLENGTH
long x[64],y[64];         // input and output arrays for FFT
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);


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
  int myPin = 0x01;
	int index;
	unsigned short input;  
	unsigned static long LastTime;  // time at previous ADC sample
	unsigned long thisTime;         // time at current ADC sample
	long jitter;                    // time between measured and expected
  if(NumSamples < RUNLENGTH){   // finite time run
    toggle(myPin);
    input = ADC_In(1);
    toggle(myPin);
    thisTime = OS_Time();       // current time, 20 ns
    toggle(myPin);
    DASoutput = Filter(input);
    toggle(myPin);
    FilterWork++;        // calculation finished
    toggle(myPin);
    if(FilterWork>1){    // ignore timing of first interrupt
      toggle(myPin);
      jitter = OS_TimeDifference(100000, OS_TimeDifference(LastTime,thisTime));  // in usec
      toggle(myPin);
      if(jitter > MaxJitter){
        toggle(myPin);
        MaxJitter = jitter;
      }
      toggle(myPin);
      if(jitter < MinJitter){
        toggle(myPin);
        MinJitter = jitter;
      }        // jitter should be 0
      toggle(myPin);
      index = jitter+JITTERSIZE/2;   // us units
      toggle(myPin);
      if(index<0)index = 0;
      toggle(myPin);
      if(index>=JitterSize) index = JITTERSIZE-1;
      toggle(myPin);
      JitterHistogram[index]++;
      toggle(myPin);
    }
    LastTime = thisTime;
    toggle(myPin);
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
  if(OS_AddThread(&ButtonWork,100,4)){
    NumCreated++; 
  }
}
//--------------end of Task 2-----------------------------

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
  int myPin = 0x02;
unsigned long data,DCcomponent; // 10-bit raw ADC sample, 0 to 1023
unsigned long t;  // time in ms
unsigned long myId = OS_Id();
  toggle(myPin);
  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz
  toggle(myPin);
  NumCreated += OS_AddThread(&Display,128,0); 
  toggle(myPin);
  while(NumSamples < RUNLENGTH) {
    toggle(myPin);
    for(t = 0; t < 64; t++){   // collect 64 ADC samples
      toggle(myPin);
      data = OS_Fifo_Get();    // get from producer
      toggle(myPin);
      x[t] = data;             // real part is 0 to 1023, imaginary part is 0
    }
    cr4_fft_64_stm32(y,x,64);  // complex FFT of last 64 ADC values
    toggle(myPin);
    DCcomponent = y[0]&0xFFFF; // Real part at frequency 0, imaginary part should be zero
    toggle(myPin);
    OS_MailBox_Send(DCcomponent);
    toggle(myPin);
  }
  OS_Kill();  // done
  OS_Delay(OS_ARBITRARY_DELAY);
}
//******** Display *************** 
// foreground thread, accepts data from consumer
// displays calculated results on the LCD
// inputs:  none                            
// outputs: none
void Display(void){ 
unsigned long data,voltage;
  char str[20];
   sprintf(str, "Run length is %d", RUNLENGTH/1000);
  OLED_Out(TOP, str);   // top half used for Display
  while(NumSamples < RUNLENGTH) {
  //  UART_OutString("Started Display\r\n");
    sprintf(str, "Time left is %d", (RUNLENGTH-NumSamples)/1000);
    //UART_OutString("Display A\r\n");
    OLED_Out(TOP, str);   // top half used for Display
    //UART_OutString("Display B\r\n");
    data = OS_MailBox_Recv();
    //UART_OutString("Display C\r\n");
    voltage = 3000*data/1024;               // calibrate your device so voltage is in mV
    sprintf(str, "v(mV) = %d", voltage);
    //UART_OutString("Display D\r\n");
    OLED_Out(TOP, str);
    //UART_OutString("Finished Display\r\n");
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
short PID_stm32(short Error, short *Coeff);
short Actuator;
void PID(void){ 
  int myPin = 0x04;
  short err;  // speed error, range -100 to 100 RPM
  unsigned long myId = OS_Id();
  toggle(myPin);
  PIDWork = 0;
  IntTerm = 0;
  PrevError = 0;
  Coeff[0] = 384;   // 1.5 = 384/256 proportional coefficient
  Coeff[1] = 128;   // 0.5 = 128/256 integral coefficient
  Coeff[2] = 64;    // 0.25 = 64/256 derivative coefficient*
  while(NumSamples < RUNLENGTH) {
    for(err = -1000; err <= 1000; err++){    // made-up data
      toggle(myPin);
      Actuator = PID_stm32(err,Coeff)/256;
      toggle(myPin);
    }
    PIDWork++;        // calculation finished
    toggle(myPin);
  }
  for(;;){toggle(myPin);}          // done
}
//--------------end of Task 4-----------------------------

//------------------Task 5--------------------------------
// UART background ISR performs serial input/output
// two fifos are used to pass I/O data to foreground
// Lab 1 interpreter runs as a foreground thread
// the UART driver should call OS_Wait(&RxDataAvailable) when foreground tries to receive
// the UART ISR should call OS_Signal(&RxDataAvailable) when it receives data from Rx
// similarly, the transmit channel waits on a semaphore in the foreground
// and the UART ISR signals this semaphore (TxRoomLeft) when getting data from fifo
// it executes a digital controller 
// your intepreter from Lab 1, with additional commands to help debug 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
//void Interpreter(void);    // just a prototype, link to your interpreter
// add the following commands, leave other commands, if they make sense
// 1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork
      
// 2) print debugging parameters 
//    i.e., x[], y[] 
//--------------end of Task 5-----------------------------

//*******************final user main DEMONTRATE THIS TO TA**********
int main(void){
  #if PROFILING == 1
    volatile unsigned long delay;
  #endif
//  OS_Init();           // initialize, disable interrupts
//  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  OLED_Init(15);
  ADC_Init(1000);
  ADC_Open(0);
  ADC_Open(1);
  OS_Init();
  SH_Init();
  
  #if PROFILING
    // intialize port b pins as specified by PINS mask
    // for digital output for use in profiling threads
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
    delay = SYSCTL_RCGC2_R;
    GPIO_PORTB_DIR_R |= PINS;
    GPIO_PORTB_DEN_R |= PINS;
    GPIO_PORTB_DATA_R &= ~PINS;
  #endif

  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;
  MaxJitter = 0;       // OS_Time in 20ns units
  MinJitter = 10000000;

//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(4);    // ***note*** 4 is not big enough*****

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_Add_Periodic_Thread(&DAS,2,1); // 2 kHz real time sampling

  NumCreated = 0 ;
  FilterWork = 0;
// create initial foreground threads
  NumCreated += OS_AddThread(&SH_Shell,128,2); 
  NumCreated += OS_AddThread(&Consumer,128,1); 
  NumCreated += OS_AddThread(&PID,128,3); 
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//+++++++++++++++++++++++++DEBUGGING CODE++++++++++++++++++++++++
// ONCE YOUR RTOS WORKS YOU CAN COMMENT OUT THE REMAINING CODE
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
unsigned long Count4;   // number of times thread4 loops
unsigned long Count5;   // number of times thread5 loops
void Thread1(void){
  Count1 = 0;          
  for(;;){
    Count1++;
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread2(void){
  Count2 = 0;          
  for(;;){
    Count2++;
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread3(void){
  Count3 = 0;          
  for(;;){
    Count3++;
    OS_Suspend();      // cooperative multitasking
  }
}
OS_SemaphoreType* Free;       // used for mutual exclusion

int testmain1(void){ 
  OS_Init();           // initialize, disable interrupts
  SH_Init();
//  EnableInterrupts();
//  SH_Shell();
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&SH_Shell,128,2);
   NumCreated += OS_AddThread(&Thread1,128,1); 
   NumCreated += OS_AddThread(&Thread2,128,2); 
   NumCreated += OS_AddThread(&Thread3,128,3); 
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//*******************Second TEST**********
// Once the initalize test runs, test this 
// no UART interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// no timer interrupts
// no select switch interrupts
// no ADC serial port or oLED output
// no calls to semaphores
void Thread1b(void){
  Count1 = 0;          
  for(;;){
    Count1++;
  }
}
void Thread2b(void){
  Count2 = 0;          
  for(;;){
    Count2++;
  }
}
void Thread3b(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
int testmain2(void){  // testmain2
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1b,128,1); 
  NumCreated += OS_AddThread(&Thread2b,128,2); 
  NumCreated += OS_AddThread(&Thread3b,128,3); 
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//*******************Third TEST**********
// Once the second test runs, test this 
// no UART1 interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// Timer2 interrupts, with or without period established by OS_AddPeriodicThread
// PortF GPIO interrupts, active low
// no ADC serial port or oLED output
// tests the spinlock semaphores, tests Sleep and Kill
OS_SemaphoreType* Readyc;        // set in background
int Lost;
void BackgroundThread1c(void){   // called at 1000 Hz
  Count1++;
  OS_Signal(Readyc);
}
void Thread5c(void){
  for(;;){
    OS_Wait(Readyc);
    Count5++;   // Count2 + Count5 should equal Count1 
    Lost = Count1-Count5-Count2;
  }
}
void Thread2c(void){
//  Readyc = OS_InitSemaphore(0);
  Count1 = 0;    // number of times signal is called      
  Count2 = 0;    
  Count5 = 0;    // Count2 + Count5 should equal Count1  
  NumCreated += OS_AddThread(&Thread5c,128,3); 
  OS_Add_Periodic_Thread(&BackgroundThread1c,10000,0); 
  for(;;){
    OS_Wait(Readyc);
    Count2++;   // Count2 + Count5 should equal Count1
  }
}

void Thread3c(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
void Thread4c(void){ int i;
  for(i=0;i<64;i++){
    Count4++;
    OS_Sleep(10);
  }
  OS_Kill();
  Count4 = 0;
}
void BackgroundThread5c(void){   // called when Select button pushed
  NumCreated += OS_AddThread(&Thread4c,128,3); 
}
      
int testmain3(void){   // Testmain3
  Count4 = 0;
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  OS_Init();           // initialize, disable interrupts

  NumCreated = 0 ;
  OS_AddButtonTask(&BackgroundThread5c,2);
  NumCreated += OS_AddThread(&Thread2c,128,2); 
  NumCreated += OS_AddThread(&Thread3c,128,3); 
  NumCreated += OS_AddThread(&Thread4c,128,3); 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;  // this never executes
}

//*******************Fourth TEST**********
// Once the third test runs, run this example
// Count1 should exactly equal Count2
// Count3 should be very large
// Count4 increases by 640 every time select is pressed
// NumCreated increase by 1 every time select is pressed

// no UART interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// Timer interrupts, with or without period established by OS_AddPeriodicThread
// Select switch interrupts, active low
// no ADC serial port or oLED output
// tests the spinlock semaphores, tests Sleep and Kill
OS_SemaphoreType* Readyd;        // set in background
void BackgroundThread1d(void){   // called at 1000 Hz
static int i=0;
  i++;
  if(i==50){
    i = 0;         //every 50 ms
    Count1++;
    OS_bSignal(Readyd);
  }
}
void Thread2d(void){
//  Readyd = OS_InitSemaphore(0);
  Count1 = 0;          
  Count2 = 0;          
  for(;;){
    OS_bWait(Readyd);
    Count2++;     
  }
}
void Thread3d(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
void Thread4d(void){ int i;
  for(i=0;i<640;i++){
    Count4++;
    OS_Sleep(1);
  }
  OS_Kill();
}
void BackgroundThread5d(void){   // called when Select button pushed
  NumCreated += OS_AddThread(&Thread4d,128,3); 
}
int testmain4(void){   // Testmain4
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  Count4 = 0;          
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  OS_Add_Periodic_Thread(&BackgroundThread1d,1000,0); 
  OS_AddButtonTask(&BackgroundThread5d,2);
  NumCreated += OS_AddThread(&Thread2d,128,2); 
  NumCreated += OS_AddThread(&Thread3d,128,3); 
  NumCreated += OS_AddThread(&Thread4d,128,3); 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;  // this never executes
}


