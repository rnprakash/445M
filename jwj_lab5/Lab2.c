//*****************************************************************************
//
// Lab3.c - user programs
// Jonathan Valvano, Feb 17, 2011, EE345M
//
//*****************************************************************************
// feel free to adjust these includes as needed
// You are free to change the syntax/organization of this file
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "inc/hw_types.h"
//#include "serial.h"
#include "rit128x96x4.h"
#include "adc.h"
#include "os.h"
#include "lm3s8962.h"
#include "UART2.h"
#include "Fifo.h"


unsigned long NumCreated;   // number of foreground threads created
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
unsigned long FilterDataLost;
unsigned long FilterDataAdded;
extern long MaxJitter;    // largest time difference between interrupt trigger and running thread
extern long MinJitter;    // smallest time difference between interrupt trigger and running thread
extern unsigned long const JitterSize;
extern unsigned long JitterHistogram[];
#define MAXSTRLEN 30
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units
#define PERIOD TIME_1MS/2     // 2kHz sampling period in system time units
// 10-sec finite time experiment duration 
#define RUNLENGTH 64   // display results and quit when NumSamples==RUNLENGTH
long x[64],y[64];         // input and output arrays for FFT

const char* voltage = "v";
const char* freq = "f";
const char* time = "t";
const unsigned char pxl[1] = {0xFF}; 
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);

bckgndTcb *dasTcb; //the background thread structure which holds jitter info

const long h[51]={20,-73,78,-260,280,-271,347,-745,-161,-323,322,
     400,-1138,-861,-1259,953,1337,-1582,-1004,-5710,5340,-1209,8501,-15602,-11637,
     48509,-11637,-15602,8501,-1209,5340,-5710,-1004,-1582,1337,953,-1259,-861,-1138,
     400,322,-323,-161,-745,347,-271,280,-260,78,-73,20};



 const unsigned char vert[87] = {
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
		0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0};
		
  const unsigned char horz[60] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//configuration flags
volatile char SWTRIGGER; //if 1, use software triggered interrupts for ADC. If 0, use hardware triggered.
volatile char FILTER;    //if 1, use FIR filter on data. If 0, use raw data
volatile char UARTOUT;   //set to publish ADC and FFT data to UART
volatile char DISPLAYFFT; //if 1, display fft graph on OLED. if 0, display waveform on OLED.
volatile char RUNNING;   //whether to continuously update the display
volatile char UPDATEAXIS;
Sema4Type DisplayDone;
Sema4Type FilteredDataAvail;

#define FIFOSIZE 128
#define FIFOSUCCESS 1
#define FIFOFAIL 0
typedef unsigned short FIFOTYPE;


AddPointerFifo(Filter,FIFOSIZE,FIFOTYPE,FIFOSUCCESS,FIFOFAIL);
//AddPointerFifo(Display,FIFOSIZE*2,FIFOTYPE,FIFOSUCCESS,FIFOFAIL);

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

//FIR filter: high pass software filter
Sema4Type FifoDataAvail;
short FIRFilter(short data){int cnt;
	static int idx = 0;
	static short x[52] = {0, }; // all the raw data needed for the filter, start it at 0.
	long y = 0;
	
  //if(idx == 50){ idx = 0;}  //store new data by overriding oldest data in a circular fasion
	//else{idx = idx + 1;}
  x[idx] = data - 512; 
	for(cnt = 0; cnt < 51; cnt ++){ //the array is 1 element larger than the # of samples needed. 
		y += h[cnt]*(x[idx]);         //so after the convolution is done, the index will stop at the element right after
		if(idx == 0){ idx = 51;}      //where it started. This is the new newest value, the previous is right before it and is the second newest, etc.
		else{idx = idx - 1;}          
	}
  //        <-- read values backwards     v loops around and ends here. START will be the 2nd newest value the next time around
	//[x][x][x][x][x][x][x][x]......[START][END][x]
  //                                 ^ starts here
		
	  y = (y/16384) + 512; //divide by fixed point value to get properly scaled value and add back original offset
	
	return (short) y;
}


//*******************FilterThread********************
//continously reads in raw ADC data and optionally puts it through
//and FIR filter if the FILTER flag is set.
//Republishes the data to FilterFifo.
void FilterThread(void){FIFOTYPE data;
	for(;;){
		OS_SpinWait(&FifoDataAvail);
		while(OS_Fifo_Get(&data) == 0){};
		if(FILTER){
			data = FIRFilter(data);
		}
			if(FilterFifo_Put(data) == 0){
				FilterDataLost ++;
			}
			else{
				FilterDataAdded ++;
				OS_Signal(&FilteredDataAvail);
			}
	}
	
}








//******** DAS *************** 
// background thread, calculates FIR filter
// runs 2000 times/sec
// inputs:  none
// outputs: none
unsigned short DASoutput;
void DAS(void){ 
unsigned short input;  
     // finite time run
	if(NumSamples < RUNLENGTH){
		if(SWTRIGGER == 1){
     input = ADC_In(0);
		
       if(OS_Fifo_Put(input) == 0){ DataLost++;}
			 else{ OS_SpinSignal(&FifoDataAvail);
             NumSamples++;               // number of samples
				 }
		}
	}
}
//--------------end of Task 1-----------------------------
void DrawAxes(void){ 
	OS_bWait(&OLEDFree);
  RIT128x96x4Clear();

	if(DISPLAYFFT){
//  RIT128x96x4StringDraw(voltage, 0, 48, 0xF);
	RIT128x96x4StringDraw(freq, 64, 89, 0xF);	
//  RIT128x96x4ImageDraw(vert, 7, 0, 2, 87); //drawing vertical axis
  RIT128x96x4ImageDraw(horz, 7, 87, 120, 1); //drawing horizontal axis
  } //voltage vs frequency
	else{
 // RIT128x96x4StringDraw(voltage, 0, 48, 0xF);
	RIT128x96x4StringDraw(time, 64, 89, 0xF);	
//  RIT128x96x4ImageDraw(vert, 7, 0, 2, 87); //drawing vertical axis
  RIT128x96x4ImageDraw(horz, 7, 87, 120, 1); //drawing horizontal axis
   //voltage vs time
 }
 OS_bSignal(&OLEDFree);

}
//------------------Task 2--------------------------------
// background thread executes with select button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************
void ButtonWork(void){
  DISPLAYFFT ^= 0x1; //toggle display type
	DrawAxes();
  OS_Kill();  // done
} 

void DownWork(void){
	
	RUNNING ^= 0x1; //toggle disply updating
		
	OS_Kill();
}

//************ButtonPush*************
// Called when Select Button pushed
// Adds another foreground task
// background threads execute once and return
void ButtonPush(void){
  if(OS_AddThread(&ButtonWork,100,0)){
    NumCreated++; 
  }
}
//************DownPush*************
// Called when Down Button pushed
// Adds another foreground task
// background threads execute once and return
void DownPush(void){
     RUNNING ^= 0x01; 
  
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
	if(NumSamples < RUNLENGTH){
		if(SWTRIGGER == 0){
			if(OS_Fifo_Put(data) == 0){ DataLost++;}
			else{       
				NumSamples++;               // number of samples
        OS_SpinSignal(&FifoDataAvail);
				}
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
OSFIFOTYPE data;
//unsigned long DCcomponent; // 10-bit raw ADC sample, 0 to 1023
unsigned long t;  // time in ms
unsigned long myId = OS_Id(); 
	for(;;){
		
		OS_bSpinWait(&DisplayDone);
    for(t = 0; t < 64; t++){   // collect 64 ADC samples
			OS_Wait(&FilteredDataAvail);
      while(FilterFifo_Get(&data) == FIFOFAIL){};    // get from producer
      x[t] = data;             // real part is 0 to 1023, imaginary part is 0
    }
    cr4_fft_64_stm32(y,x,64);  // complex FFT of last 64 ADC values
		
// 		for(t = 0; t <64; t++){ //send FFT input and output values through FIFO to display
// 			DisplayFifo_Put(x[t]);
// 			DisplayFifo_Put((y[t]&0xFFFF));
// 		}

    NumCreated += OS_AddThread(&Display,128,0); 
	 while(!RUNNING){} //stop running if pause flag is set
	 NumSamples = 0;

	}
}


void drawPointColumn(unsigned long ulX, unsigned long ulY){
	unsigned char column[86] = {0,};
	if(ulY > 85){column[85] = 0xFF;}
	else{
  column[ulY] = 0xFF;
	}
  RIT128x96x4ImageDraw(column, ulX, 0, 2, 86);
	
}

void drawColumn(unsigned long ulX, unsigned long ulY){int i;
		unsigned char column[86] = {0,};
	if(ulY > 85){i = 85;}
  else{i = ulY;}

	for(; i < 86; i++){
		column[i] = 0xFF;
	}
	  RIT128x96x4ImageDraw(column, ulX, 0, 2, 86);

	
}


//******** Display *************** 
// foreground thread, accepts data from consumer
// displays FFT results in UART
// inputs:  none                            
// outputs: none
void Display(void){ int i;
//unsigned short v[64];
//short fft[64];
	short fftVal;
	short fftVali;
	unsigned short voltageVal;
//    for(i = 0; i < 64; i ++){ //collect output data from consumer
// 	  while(DisplayFifo_Get(&v[i]) == FIFOFAIL){} //get voltage value from fifo
// 		while(DisplayFifo_Get(&fft[i]) == FIFOFAIL){} //get frequency value from fifo			
// 	 }
		
	 if(UARTOUT == 1){
		for(i = 0; i < 64; i ++){
			UART_OutUDec(x[i]);
			UART_OutString(", ");
       fftVal = (short) (y[i]&0xFFFF);
			 fftVali = (short) (y[i]>>16);
       fftVal = sqrt(fftVal*fftVal + fftVali*fftVali);  
			UART_OutUDec(fftVal);
			UART_OutChar(CR);
			UART_OutChar(LF);
		}
		UARTOUT = 0;
	}else{
		if(UPDATEAXIS){DrawAxes(); UPDATEAXIS = 0;}
		OS_bWait(&OLEDFree);	
		if(DISPLAYFFT){
			for(i = 0; i < 32; i += 1){
 			 fftVal = (short) (y[i]&0xFFFF);
			 fftVali = (short) (y[i]>>16);
       fftVal = sqrt(fftVal*fftVal + fftVali*fftVali); 
       fftVal = 10*log(fftVal);				
       fftVal = 86 - (fftVal);
				
       drawColumn((unsigned long) (i*4), (unsigned long) fftVal);		
       drawColumn((unsigned long) (i*4)+2, (unsigned long) fftVal);
			}

		}else{
			for(i = 0; i < 64; i++){
       voltageVal = 87 - (x[i]/12);    
       drawPointColumn((unsigned long) i*2, (unsigned long) voltageVal);
			}				
		}
// 	 for(i = 0; i < 64; i ++){
// 		 
// 		 //draw data points on graph
// 		 if(DISPLAYFFT){ //display voltage vs freqeuncy
// 			 fftVal = (short) (y[i]&0xFFFF);
// 			 fftVali = (short) (y[i]>>16);
//        fftVal = sqrt(fftVal*fftVal + fftVali*fftVali);  
//        fftVal = 86 - (fftVal/12);
//        RIT128x96x4ImageDraw(pxl, i*2, fftVal, 2, 1);				 
//      }
// 		 else{ //display voltage vs time
//        voltageVal = 87 - (x[i]/12);    
//       RIT128x96x4ImageDraw(pxl, i*2, voltageVal, 2, 1);
//      }
// 	 }
   OS_bSignal(&OLEDFree);
  }
	OS_bSpinSignal(&DisplayDone);
 OS_Kill();	
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
  for(;;){ }          // done
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
//extern void Interpreter(void);    // just a prototype, link to your interpreter
// add the following commands, leave other commands, if they make sense
// 1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork
      
// 2) print debugging parameters 
//    i.e., x[], y[] 

int process_cmd(char *input){
	static int screen1Line;
	unsigned short adc_val;
	char *strptr;
//	char inString1[MAXSTRLEN];
//	char inString2[MAXSTRLEN];
	int i;
	
		if(strncmp(input, "adcopen", 7) == 0){ //strncmp(str1, str2, maxlen) returns 0 if the same
			ADC_Open();
			return 1;
		}
		
		if(strncmp(input, "adcin", 5) == 0){
			adc_val = ADC_In(3);			
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutUDec((long) adc_val);
			return 1;
		}
		
		if(strncmp(input, "print ", 6) == 0){
			strptr = input + 6;  //get string value after 'print ' command
			OLED_TxtMessage (0, screen1Line, strptr);
			screen1Line = (screen1Line + 1) %4;
		 return 1;
    }
		
			if(strncmp(input, "cheermeup", 9)== 0){
			OLED_TxtMessage(0, screen1Line, ":-) hang in there!");
			screen1Line = (screen1Line + 1) %4;

			return 1;
		}
	
//		 1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork
		if(strncmp(input, "print_perf", 10)==0){

        UART_OutChar(CR);
			  UART_OutChar(LF);
			  UART_OutString("PIDWork    =");
			  UART_OutUDec(PIDWork);
			  UART_OutChar(CR);
			  UART_OutChar(LF);
				UART_OutString("DataLost   =");
			  UART_OutUDec(DataLost);
			  UART_OutChar(CR);
			  UART_OutChar(LF);
		  	UART_OutString("Jitter(us) =");
			  UART_OutUDec(MaxJitter-MinJitter);
			  UART_OutChar(CR);
			  UART_OutChar(LF);
        UART_OutString("NumSamples    =");
			  UART_OutUDec(NumSamples);
			  UART_OutChar(CR);
			  UART_OutChar(LF);
				UART_OutString("NumCreated   =");
			  UART_OutUDec(NumCreated);
			  UART_OutChar(CR);
			  UART_OutChar(LF);
		  	UART_OutString("FilterWork =");
			  UART_OutUDec(FilterWork);
			  UART_OutChar(CR);
			  UART_OutChar(LF);

			
			return 1;
		}
		if(strncmp(input, "print_debug", 11)==0){
        UART_OutChar(CR);
			  UART_OutChar(LF);
			  UART_OutString("X:              Y:");		
       
			for(i = 0; i < 64; i++){
        UART_OutChar(CR);
			  UART_OutChar(LF);
				UART_OutUDec(x[i]);
				UART_OutString("                      ");
				UART_OutUDec(y[i]&0x0000FFFF);
       
     }
			
			
			return 1;
		}
		
		if(strncmp(input, "displayIntInfo", 17) == 0){
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Max time with interrupts disabled: ");
		  UART_OutUDec(OS_MaxCritical());
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Percentage of time with interrupts disabled: ");
			UART_OutUDec(OS_PercentCritical());
			UART_OutString(" out of 1000");

			
			return 1;
		}
		
		if(strncmp(input, "clearIntInfo", 15) == 0){
			OS_ClearCriticalStats();
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Critical section data cleared.");

			
			return 1;
		}
		
		if(strncmp(input, "displayProfiling", 16) == 0){
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Thread addr:     Event code:           OS Time");
			UART_OutChar(CR);
			UART_OutChar(LF);
			for(i = 0; i < 100; i ++){
			  UART_OutUHex((unsigned long) profileData1[i]);
			  UART_OutString("                     ");
        UART_OutUHex(profileData2[i]);
				UART_OutString("              ");
				UART_OutUDec(profileData3[i]);
				UART_OutChar(CR);
			  UART_OutChar(LF);
			}
		
			
			return 1;
		}
		
		
		if(strncmp(input, "clearProfiling", 14) == 0){
			
			OS_ClearProfiling();
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Profile data cleared.");
			
			
			return 1;
		}
		
		
		if(strncmp(input, "triggerHardware", 15) == 0){
     SWTRIGGER = 0;
			
			return 1;

    } 

      
   if(strncmp(input, "triggerSoftware", 15) == 0){
    SWTRIGGER  = 1; 

		 return 1;
   }		
		
		if(strncmp(input, "enableFilter", 12) == 0){
    FILTER = 1;
			
		return 1;

    }	
		
		if(strncmp(input, "disableFilter", 13) == 0){
    FILTER = 0;
			
	  return 1;

    }	
		
		if(strncmp(input, "printSoundIn", 12) == 0){
		UARTOUT = 1;			
		return 1;
     
    }	
		
		if(strncmp(input, "graphVvsT", 9) == 0){
    DISPLAYFFT = 0;
		UPDATEAXIS = 1;
		return 1;
    }	
		
		if(strncmp(input, "graphVvsF", 9) == 0){
    DISPLAYFFT = 1;
		UPDATEAXIS = 1;
		return 1;
     
    }		
		
		return 0;
		

}





void Interpreter(void){

  char inString1[MAXSTRLEN];

	for(;;){
		UART_InString(inString1, MAXSTRLEN);
		
    process_cmd(inString1);
		
		UART_OutChar(CR);
    UART_OutChar(LF);
	}
  }



//--------------end of Task 5-----------------------------



int main(void){
	OS_Init();
	ADC_Open();
	UART_Init();
	
	DataLost = 0;        // lost data between producer and consumer
  RUNNING = 1;
  SWTRIGGER = 0;
	FILTER = 1;
	UARTOUT = 0;
	DISPLAYFFT = 0;
//	NumSamples = 0;

	
	FilterFifo_Init();
//	DisplayFifo_Init();
	OS_InitSemaphore(&FifoDataAvail, 0);
	OS_InitSemaphore(&FilteredDataAvail, 0);
	OS_InitSemaphore(&DisplayDone, 1);
	
	OS_AddThread(&FilterThread, 128, 0);
	OS_AddThread(&Consumer, 128, 0);
	//OS_AddThread(&PID, 128, 6);
	OS_AddThread(&Interpreter, 128, 1);
	OS_AddButtonTask(&ButtonPush, 1);
	OS_AddDownTask(&DownPush, 2);
	ADC_Collect(0, 40000, &Producer); // start ADC sampling, channel 0, 10,000 hz
	dasTcb = OS_AddPeriodicThread(&DAS, 83, 0); //add software triggered ADC sampling every 100 us

	DrawAxes();
	OS_Launch(50*50); //lauch with .5 ms timeslice
	
}
