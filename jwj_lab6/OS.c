// OS.c
// Runs on LM3S8962
// John Jacobellis and Nikki Verreddigari

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
  Program 7.5, example 7.6

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
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "lm3s8962.h"
#include "OS.h"
#include "FIFO.h"


void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
long OS_StartCritical(void);
void OS_EndCritical(long i);
long GetInterruptStatus(void);
void StartOS(void);
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void (*PeriodicTask)(void);  // user function
void (*ButtonTask)(void);    // user button function
void (*DownTask)(void);      // user down button function

//Timers:
//Timer0A = ADC
//Timer1B = sleep counter
//Timer2A = periodic task
//Timer2B = OSCounter


#define PF0 (*((volatile unsigned long *)0x40025004))
#define PF1 (*((volatile unsigned long *)0x40025008))

#define PE1 (*((volatile unsigned long *)0x40024008))

#define PD2 (*((volatile unsigned long *)0x40007010))
#define PD3 (*((volatile unsigned long *)0x40007020))
#define PD4 (*((volatile unsigned long *)0x40007040))
#define PD5 (*((volatile unsigned long *)0x40007080))


#define FIFOSIZE 128
#define OSFIFOSIZE 128
#define OSFIFOSUCCESS 1
#define OSFIFOFAIL 0

#define MAXSTRLEN 30
unsigned long const JitterSize=JITTERSIZE;

typedef unsigned short OSFIFOTYPE;
typedef unsigned long OSMAILBOXTYPE;
volatile unsigned long GlobalCounter;
long MaxJitter;
long MinJitter;
//variable to hold the previous state of the select button
unsigned char LastPF1;
unsigned char LastPE1;
unsigned long MaxCritical; //maximum time with interrupts disabled
unsigned long TotalCritical;
long StartCriticalTime; //time when critical section started
volatile char StartCriticalValid;
//OS Mailbox for passing a single piece of data
OSMAILBOXTYPE mailboxData;
char          mailboxFlag;



Sema4Type OLEDFree;
Sema4Type TxFIFOSpaceAvail;
Sema4Type RxFIFOSpaceAvail;
Sema4Type RxFIFODataAvail;
Sema4Type TxFIFODataAvail;
Sema4Type TxFIFOFree;
Sema4Type RxFIFOFree;
Sema4Type OSFIFODataAvail;
Sema4Type OSFIFOSpaceAvail;
Sema4Type OSFIFOFree;
Sema4Type OSMailboxEmpty;
Sema4Type OSMailboxFull;


//************Thread control structures***************
#define NUMTHREADS 50
#define STACKSIZE 128

tcb tcbs[NUMTHREADS];
tcb *RunPt;
tcb *HeadPt;
tcb *NextRunPt;
unsigned int threadCnt;
int threadIdx;
long Stacks[NUMTHREADS][STACKSIZE];


bckgndTcb bgtcbs[NUMTHREADS];
bckgndTcb *HeadPtbg;


unsigned int threadCntbg;
int threadIdxbg;

long *profileData1[100];
unsigned char profileData2[100];
long profileData3[100];
int profileIndex;
//add functions:
//void OS_Fifo_Init();
//int OS_Fifo_Put(data)
//int OS_Fifo_Get(*data);
//unsigned short OS_Fifo_Size();
AddPointerFifo(OS_,OSFIFOSIZE,OSFIFOTYPE,OSFIFOSUCCESS,OSFIFOFAIL);





//*************Timer1B_Init*************
// initializes timer1B for sleep counter
void Timer1B_Init(void){long delay;
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER1; //activate timer1
	delay = SYSCTL_RCGC1_R;

	 TIMER1_CTL_R &= ~0x00000100;     // disable timer1B during setup
	 TIMER1_CFG_R = 0x00000004;       // 2) configure for 16-bit timer mode
	 TIMER1_TBMR_R = 0x00000002;      //configure timer B for periodic mode
	 TIMER1_TBILR_R = 1000;         //configure timer B for 1 ms reload time
	 TIMER1_TBPR_R = 49;               //configure for 1us ticks
   TIMER1_ICR_R = 0x0000100;      //clear timer1B timeout flag
	 TIMER1_IMR_R |= 0x00000100;    //enable timeout interrupt
	 NVIC_PRI5_R = (NVIC_PRI5_R&0xFF00FFFF)|0x00800000;//enable interrupt 22, bits 23:21, priority 4
	 NVIC_EN0_R |= NVIC_EN0_INT22;    // 9) enable interrupt 22 in NVIC	
	 TIMER1_CTL_R |= 0x00000100;      // enable timer1B
	 GlobalCounter = 0;	
}

//sleep
void Timer1B_Handler(void){tcb *tcbPt; long i;
	i = OS_StartCritical();
	TIMER1_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge timer1B timeout
  tcbPt = HeadPt;
	do{
	  if(tcbPt->sleepState !=0){ //decrement sleepstate only if the thread is sleeping
			tcbPt->sleepState -= 1;
		}
		if(tcbPt->hasNotRun){
			tcbPt->activePriority = tcbPt->activePriority - 1;
		}else{
			tcbPt->hasNotRun = 1;
		}
		tcbPt = tcbPt->next;
	}while(tcbPt != HeadPt);
	
	OS_EndCritical(i);
	GlobalCounter++;
}

void PortF_Init(void){long delay;
	DisableInterrupts();
		SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
	  delay = SYSCTL_RCGC2_R;
   	GPIO_PORTF_DEN_R &= ~0x03;  //disable PF0 and PF 1
		GPIO_PORTF_PUR_R |= 0x02; //enable PF1 pullup resistor
  	GPIO_PORTF_DIR_R |= 0x01 ; //configure PF0 for output
	  GPIO_PORTF_DIR_R &= ~0x02; //configure PF1 for input
	  

	
	  GPIO_PORTF_AFSEL_R &= ~0x03;
    GPIO_PORTF_DEN_R |= 0x03; //enable PF0, PF1
	  PF0 = 0x00;
	
	EnableInterrupts();           // (i) Program 5.3

}

void PortE_Init(void){long delay;
	
	DisableInterrupts();
	SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE;
	delay = SYSCTL_RCGC2_R;
	GPIO_PORTE_DEN_R &= ~0x02; //disable PE1
	GPIO_PORTE_DIR_R &= ~0x02;  //set PE1 to input
	GPIO_PORTE_PUR_R |= 0x02; //enable PE1 pullup resistor

	GPIO_PORTE_AFSEL_R &= ~0x02; //disable alternate function on PE1

	GPIO_PORTE_DEN_R |= 0x02;    //enable PE1
	
	EnableInterrupts();
	
	
}
void PortD_Init(void){long delay;
		DisableInterrupts();

		SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOD; // activate port D
	  delay = SYSCTL_RCGC2_R;
   	GPIO_PORTD_DEN_R &= ~0x3C;  //disable PD2 - PD5
  	GPIO_PORTD_DIR_R |= 0x3C ; //configure PD2-PD5 for inputs
	  GPIO_PORTD_AFSEL_R &= ~0x3C;
    GPIO_PORTD_DEN_R |= 0x03C; //enable PD2-PD5
	 
	
		EnableInterrupts();           // (i) Program 5.3

}


void static DebounceTask(void){
	OS_Sleep(3);
	LastPF1 = PF1;
	GPIO_PORTF_ICR_R = 0x02;
	GPIO_PORTF_IM_R |= 0x02;
	OS_Kill();
}
void static DebounceDownTask(void){
	OS_Sleep(3);
	LastPE1 = PE1;
	GPIO_PORTE_ICR_R = 0x02;
	GPIO_PORTE_IM_R |= 0x02;
	OS_Kill();
}	
	
void GPIOPortE_Handler(void){
	if(LastPE1 != 0){
		(*DownTask)();
	}
	GPIO_PORTE_IM_R &= ~0x02;
	OS_AddThread(&DebounceDownTask, 128, 0);
	
}

void GPIOPortF_Handler(void){
	if(LastPF1 != 0){
		(*ButtonTask)();
	}
  GPIO_PORTF_IM_R &= ~0x02;
  OS_AddThread(&DebounceTask, 128, 0);	
	
}

//****************** CalcJitter ***********************

void CalcJitter(bckgndTcb *curr){ long jitter; int index;
 if(curr->lastTime != 0xFFFFF){  
				 
		      jitter = Jitter(curr->lastTime, curr->currTime, curr->period);
		 
	        if(jitter > curr->MaxJitter){
            curr->MaxJitter = jitter;
          }
          else if(jitter < curr->MinJitter){
            curr->MinJitter = jitter;
						
          }        // jitter should be 0
          
					index = jitter+JITTERSIZE/2;   // us units
          
          if(index<0){index = 0;}
          if(index>=JitterSize){index = JITTERSIZE-1;}
          curr->JitterHistogram[index]++; 
	    }
      curr->lastTime = curr->currTime;

}

// ***************** Timer2A_Init ****************
// Activate Timer2A interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in usec
// Outputs: none
void Timer2A_Init(void(*task)(void), unsigned short period){ 
	DisableInterrupts();
	SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  //SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2; // 0) activate timer2
  PeriodicTask = task;             // user function 
	

	
  TIMER2_CTL_R &= ~0x00000001;     // 1) disable timer2A during setup
  TIMER2_CFG_R = 0x00000007;       // 2) configure for 16-bit timer mode
  TIMER2_TAMR_R = 0x00000002;      // 3) configure for periodic mode
  TIMER2_TAILR_R = (period);       // 4) reload value, period in 2*us
  TIMER2_TAPR_R = 99;              // 5) 2us timer2A
  TIMER2_ICR_R = 0x00000001;       // 6) clear timer2A timeout flag
  TIMER2_IMR_R |= 0x00000001;      // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x40000000; // 8) priority 2
  NVIC_EN0_R |= NVIC_EN0_INT23;    // 9) enable interrupt 23 in NVIC

	
	

	
  EnableInterrupts();
}

//initialize timer 2b for OS counter
void Timer2B_Init(void){ long delay;
	  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2; //activate timer2
    delay = SYSCTL_RCGC1_R;
	
	  TIMER2_CTL_R &= ~0x00000100;     // disable timer2B during setup
    TIMER2_CFG_R = 0x00000007;       // 2) configure for 16-bit timer mode
	  TIMER2_TBMR_R = 0x00000002;      //configure timer B for periodic mode
	  TIMER2_TBILR_R = 0xFFFFFFFF;         //configure timer B for maximum reload time
	  TIMER2_TBPR_R = 4;               //configure for 100ns ticks
	  TIMER2_IMR_R &= ~(0x00000700);   //disable timer2B interrrupts
    TIMER2_CTL_R |= 0x00000100;      // enable timer2B

}


void Timer2A_Handler(void){
	bckgndTcb *curr;
//	bckgndTcb *nextToExec;
	int smallestPeriod = 0;
	long oldtime = 0;
	long offset = 0;
  curr = HeadPtbg;
	oldtime = OS_Time();
	
	PF0 ^= 0x01;


	//1)Iterate through the linked list and excute tasks that have 0's
 while(curr != 0){
  if(curr->timeTilExec <= 0){
	  curr->currTime = OS_Time();
		//set the timer priority the the thread that's about to be run (so other interrupt's priorities will work properly)
	  //NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|((curr->priority&0x7)<<29);

		if(profileIndex < 100){
	    profileData1[profileIndex] = (long *)curr->task; //store thread that is starting
	    profileData2[profileIndex] = 0x2; //code 2 is periodic thread starting
			profileData3[profileIndex] = OS_Time();
	    profileIndex ++;
	    }
    (*(curr->task))(); 		// execute user task	 
			if(profileIndex < 100){
	    profileData1[profileIndex] = (long *)curr->task; //store thread that is starting
	    profileData2[profileIndex] = 0x4; //code 4 is periodic thread finishing
			profileData3[profileIndex] = OS_Time();
	    profileIndex ++;
	    }
		
		  CalcJitter(curr);
		  curr->timeTilExec = curr->period;	  
      }
      curr = curr->next;
   }
	 
	 
	//2)Iterate through the linked list and find the smallest period 
	curr = HeadPtbg;
	smallestPeriod = curr->timeTilExec;
	while(curr != 0){
    if (curr->timeTilExec < smallestPeriod){
        smallestPeriod = curr->timeTilExec;
			  //nextToExec = curr;
    }
    curr = curr->next;	
  }
	

	//3)Once you find the smallest period, subtract the period from timeTilExec
	curr = HeadPtbg;
	while(curr != 0){
    curr->timeTilExec = (curr->timeTilExec - smallestPeriod);
		
    curr = curr->next;

  }
	
	//change the timer2A priority to match the next thread to execute's priority
	 // NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|((nextToExec->priority&0x7)<<29);

	
	offset = OS_TimeDifference(OS_Time(), oldtime);
	if((offset/20) >= (smallestPeriod)){
      TIMER2_TAILR_R = 0;
  }
	else{

  TIMER2_TAILR_R = smallestPeriod - (offset/20);       // reset the reload value, period in 2*us
	}
	TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer0A timeout
	PF0 ^= 0x01;

}

void PendSV_Trigger(void){
	NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; //set pendSV  pending with bit 28 of interrupt control register
}


void SysTick_Handler(void){long delay;

  tcb *tempPt;
  tcb *startPt;
  int max = 0xffff;

	OS_DisableInterrupts();
	//********priority scheduler
  tempPt = RunPt->next;
  startPt = RunPt->next;  
	NextRunPt = startPt;
  do{

    //if thread is not sleeping, then check its pri with max and set it if is more important
    if((tempPt->activePriority < max) && (tempPt->sleepState == 0)){
      max = tempPt->activePriority;
      NextRunPt = tempPt;
    }   
    tempPt= tempPt->next;

  }while(tempPt != startPt);
	
	//**************round robin scheduler
// 	tempPt = RunPt->next;
// 	while(tempPt->sleepState != 0){ tempPt = tempPt->next;}
// 	NextRunPt = tempPt;

	OS_EnableInterrupts();
	if(profileIndex < 100){
	profileData1[profileIndex] = (long *)NextRunPt; //store thread that is starting
	profileData2[profileIndex] = 0x1; //code 1 means new foreground thread is running
	profileData3[profileIndex] = OS_Time();
	profileIndex ++;
	}
	
	NextRunPt->hasNotRun = 0; //indicate that this thread has been run (for aging)
	NextRunPt->activePriority = NextRunPt->priority;
  PendSV_Trigger();
	delay = max;

}


void OS_ClearMsTime(void){
	unsigned long i;
	i = OS_StartCritical();
	GlobalCounter = 0;
	OS_EndCritical(i);
}

unsigned long OS_MsTime(void){
	return GlobalCounter;
}


//Adds the current thread to the blocked list of the given semaphore and suspends the thread
void block_Thread(Sema4Type *s){long i, delay;
		i = OS_StartCritical();

	if(s->size == 0){ //if empty list
    s->size = 1;
		if(RunPt == HeadPt){HeadPt = HeadPt->next;}
			s->HeadPt = RunPt;
			s->TailPt = RunPt;
			RunPt->prev->next = RunPt->next; //remove current thread from active linked list
			RunPt->next->prev = RunPt->prev;

		}else{
			s->size+= 1;
			if(RunPt == HeadPt){HeadPt = HeadPt->next;}
			RunPt->prev->next = RunPt->next; //take out RunPt from active LL
			RunPt->next->prev = RunPt->prev;
			RunPt->prev = s->TailPt; 
		  s->TailPt->next = RunPt; //add RunPt to blocked linked list
		  s->TailPt = RunPt;
		}
		
		OS_EndCritical(i);
		OS_Suspend();
		delay = i;

}

//removes the first thread in a given semaphor's linked list and puts it back in the active thread scheduler
//if the semaphore is not blocking any threads, nothing happens.
void unBlock_Thread(Sema4Type *s){tcb *thread; long i;
	i = OS_StartCritical();
	if(s->size > 0){ //
		s->size = s->size - 1;
		//add first thread in linked list back into active threads
		thread = s->HeadPt; //thread to move is first thread in linked list
		s->HeadPt = thread->next; //take out thread from blocked linked list
		thread->prev = RunPt; //add thread to execute next
		thread->next = RunPt->next;
		RunPt->next->prev = thread;
		RunPt->next = thread;
		
		if(s->size == 0){ //if the removed thread was the only thread blocked
			s->TailPt = 0; //reset TailPt and HeadPt to 0
			s->HeadPt = 0;
		}
		
	}
	
	OS_EndCritical(i);
}

void OS_Wait(Sema4Type *s){long i, delay;
  i = OS_StartCritical();
  while(s->value <= 0){
    OS_EnableInterrupts();
//				PD6 ^= 0x40;
		block_Thread(s);	
    delay = s->value;		
    OS_DisableInterrupts();
  }
	s->value = s->value - 1;
	OS_EndCritical(i);

}

void OS_SpinWait(Sema4Type *s){
	  OS_DisableInterrupts();
  while(s->value <= 0){
    OS_EnableInterrupts();
				//PD6 ^= 0x40;
    OS_DisableInterrupts();
  }
	s->value = s->value - 1;
	OS_EnableInterrupts();
}



void OS_Signal(Sema4Type *s){long delay;
  long status;
	status = OS_StartCritical();
	s->value = s->value + 1;
	unBlock_Thread(s);
	delay = s->value;
	OS_EndCritical(status);

}

void OS_SpinSignal(Sema4Type *s){
	long status;
	status = StartCritical();
	s->value = s->value + 1;
	EndCritical(status);
	
}

void OS_bWait(Sema4Type *s){
  long status;
  status = OS_StartCritical();
	while((s->value) == 0) {
  OS_EnableInterrupts();
		block_Thread(s);
	OS_DisableInterrupts();
  }
	s->value = 0;
	OS_EndCritical(status);
}

void OS_bSpinWait(Sema4Type *s){
	long status;
  status = StartCritical();
	while((s->value) == 0) {
  EnableInterrupts();
	DisableInterrupts();
  }
	s->value = 0;
	EndCritical(status);
	
}

void OS_bSignal (Sema4Type *s){long i;
	i = OS_StartCritical();
  s->value = 1;
	unBlock_Thread(s);
	OS_EndCritical(i);
}

void OS_bSpinSignal(Sema4Type *s){
	s->value = 1;
}

void OS_InitSemaphore(Sema4Type *s, int initalValue){
	
	s->value = initalValue;
	s->HeadPt = 0; //clear blocked list
	s->TailPt = 0;
	s->size = 0;

}


/*
void OS_Wait(Sema4Type *s){
  DisableInterrupts();
  while(*s <= 0){
    EnableInterrupts();
    DisableInterrupts();
  }
	*s = *s - 1;
	EnableInterrupts();

}

void OS_Signal(Sema4Type *s){
  long status;
	status = StartCritical();
	*s = *s + 1;
	EndCritical(status);

}

void OS_bWait(Sema4Type *s){
  long status;
  status = StartCritical();
	while((*s) == 0) {
  EnableInterrupts();
	DisableInterrupts();
  }
	*s = 0;
	EndCritical(status);
}

void OS_bSignal (Sema4Type *s){
  *s = 1;
}

void OS_InitSemaphore(Sema4Type *s, int initalValue){
	
	long status;
  status = StartCritical();
	
	*s = initalValue;
	
	EndCritical(status);
}
*/




// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, select switch and timer2 
// input:  none
// output: none
void OS_Init(void){int i; long delay;
   // set processor clock to 50 MHz
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL |
                 SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
	delay = i;
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0xFF00FFFF)|0x00E00000;//set priority 7 in SYSPRI3 bits 23:21 for PendSV handler

	MaxCritical = 0;
	TotalCritical = 0;
	profileIndex = 0;
	OS_Fifo_Init();
	PortF_Init();
	PortD_Init();
	PortE_Init();
 
	Timer2B_Init();

	
	
	for(i=0; i < NUMTHREADS; i++){ //initialize all tcb's to free
		tcbs[i].free = 1;
	}
		
	OS_InitSemaphore(&OLEDFree, 1);
  OS_InitSemaphore(&TxFIFOFree, 1);
  OS_InitSemaphore(&RxFIFOFree, 1);
  OS_InitSemaphore(&TxFIFOSpaceAvail, FIFOSIZE);
  OS_InitSemaphore(&RxFIFOSpaceAvail, FIFOSIZE);
  OS_InitSemaphore(&RxFIFODataAvail, 0);
  OS_InitSemaphore(&TxFIFODataAvail, 0);
	OS_InitSemaphore(&OSFIFODataAvail, 0);
	OS_InitSemaphore(&OSFIFOSpaceAvail, OSFIFOSIZE);
	OS_InitSemaphore(&OSFIFOFree, 1);
	OS_InitSemaphore(&OSMailboxFull, 0);
	OS_InitSemaphore(&OSMailboxEmpty, 1);
}


void SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}





char OS_Id(void){
	return RunPt->id;
	
}





//**********************************
//creates a thread with the given user function. 
//task: pointer to function to execute as a thread
//the other arguments are not used for now
int OS_AddThread(void(*task)(void), int stackSize, int pri){long status; int i;
	 static int maxPri;
	
	if(threadCnt == 0){ //adding first thread, which is the only time there will be 0 threads.
		status = OS_StartCritical();

		threadCnt++;
		SetInitialStack(0); Stacks[0][STACKSIZE-2] = (long)(task);
		RunPt = &tcbs[0]; 
		HeadPt = RunPt;      //initialize linked list
		tcbs[0].next = RunPt; //first tcb, circular pointers point to itself
		tcbs[0].prev = RunPt;
		tcbs[0].free = 0;    //tcb is now in use
		tcbs[0].sleepState = 0;  //thread is awake
		tcbs[0].priority = pri;
		maxPri = pri;
		
		OS_EndCritical(status);
		return 1;
		
	}else if(threadCnt < NUMTHREADS){
		status = OS_StartCritical();
		threadCnt ++;
		
		//find available tcb space
		for(i = 0; i < NUMTHREADS; i ++){
			if(tcbs[i].free == 1){ break;}
		}
		
		SetInitialStack(i); Stacks[i][STACKSIZE-2] = (long)(task);
		//initialize tcb to end of linked list

		tcbs[i].next = HeadPt;
		tcbs[i].prev = HeadPt->prev;
		HeadPt->prev->next = &tcbs[i];
		HeadPt->prev = &tcbs[i];
		
		tcbs[i].free = 0; //tcb is now in use
		tcbs[i].sleepState = 0; //tcb is awake
		tcbs[i].priority = pri;
		
		if(pri < maxPri){ //if the added thread is the highest priority, set it to be run first
			maxPri = pri;
			RunPt = &tcbs[i];
		}
		
		OS_EndCritical(status);
		return 1;
		
	}else{
    return 0;
  }

}



///******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 20ns clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(unsigned long theTimeSlice){long delay;
	TIMER2_CTL_R |= 0x00000001;      // 10) enable timer2A
	Timer1B_Init();                 //enable sleep timer
	delay = NVIC_ST_RELOAD_R;
  NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  NVIC_ST_CTRL_R = 0x00000007; // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}


//return current clock time. Returns a long, but the value is only 16 bits because a 16 bit timer was used
long OS_Time(void){
	return (long) (TIMER2_TBR_R) ;
}

//returns the difference between newTime and oldTime (they should both only be 16 bits) in units of 100ns
long OS_TimeDifference(long newTime, long oldTime){
	//the timer is down counting, so new time should be smaller than old time unless the counter rolled over
	if(oldTime > newTime){
		return (long) (oldTime - newTime);
	}else{ return (long) (0x0000FFFF -  (newTime - oldTime)); }
}

//returns the maximum time the OS was running with interrupts disabled
unsigned long OS_MaxCritical(void){
	return MaxCritical;
}

// returns the percentage of time the OS was running without interrupts in units of 1/100th of a percent
unsigned long OS_PercentCritical(void){
	return (TotalCritical/GlobalCounter);
}


//clears all data relating to time in critical sections
void OS_ClearCriticalStats(void){
  MaxCritical = 0;
	TotalCritical = 0;
	OS_ClearMsTime();
}

// adds a given task to the timer2A interrupt, so it is run in the background every given period
bckgndTcb* OS_AddPeriodicThread(void(*task)(void), unsigned short period, int pri){long status; int i; int j;
	bckgndTcb *curr;
  bckgndTcb *prev;
	if(threadCntbg == 0){ //adding first thread, which is the only time there will be 0 threads.
		status = OS_StartCritical();

		threadCntbg++;
		HeadPtbg = &bgtcbs[0]; //initialize the linked list
		bgtcbs[0].next = 0; //first tcb, linked list 
		bgtcbs[0].task = task;
		bgtcbs[0].period = period;
	  bgtcbs[0].timeTilExec = period;
	  bgtcbs[0].lastWaitTime = 0;
		bgtcbs[0].priority = pri;
		bgtcbs[0].lastTime = 0x000FFFFF; //ignoring first jitter value 
		 //initialized to this number because max value of OS_Time is a 
		 //short(16 bits)... so initalize it to an invalid value 
		 //because we want ignore the first jittertime value
     bgtcbs[0].MinJitter = 0;
     bgtcbs[0].MaxJitter = 0;
		bgtcbs[0].currTime = 0;
		 for(i =0; i < JITTERSIZE; i++){
         bgtcbs[threadCntbg].JitterHistogram[i] = 0;
    }
		OS_EndCritical(status);
		
   	Timer2A_Init(task, period);
		return &bgtcbs[0];
		
	}else if(threadCntbg < NUMTHREADS){
		status = OS_StartCritical();
		threadCntbg++;
		 curr = HeadPtbg;
     prev = 0;		
	
		while((pri >= curr->priority)&&(curr != 0)){
			 prev = curr;
       curr = curr->next;
      }
			if(prev == 0){
      HeadPtbg = &bgtcbs[threadCntbg];
			bgtcbs[threadCntbg].next = curr;
     }
		 else{
			prev->next = &bgtcbs[threadCntbg];
			bgtcbs[threadCntbg].next = curr;
		 }
			bgtcbs[threadCntbg].task = task;
		  bgtcbs[threadCntbg].period = period;
	    bgtcbs[threadCntbg].timeTilExec = period;
	    bgtcbs[threadCntbg].lastWaitTime = 0;
		  bgtcbs[threadCntbg].priority = pri;  
      bgtcbs[threadCntbg].lastTime = 0x000FFFFF; //ignoring first jitter value 
		 //initialized to this number because max value of OS_Time is a 
		 //short(16 bits)... so initalize it to an invalid value 
		 //because we want ignore the first jittertime value
      bgtcbs[threadCntbg].MinJitter = 0;
      bgtcbs[threadCntbg].MaxJitter = 0;
		 bgtcbs[threadCntbg].currTime = 0;
     for(j =0; j < JITTERSIZE; j++){
         bgtcbs[threadCntbg].JitterHistogram[j] = 0;
    }
		
		OS_EndCritical(status);
		return &bgtcbs[threadCntbg];
		
	}else{
    return 0;
  }

	
	
	
}

//****************OS_Suspend**************
//stops execution of the current thread and moves to the next thread
void OS_Suspend(void){long delay;
	tcb *tempPt;
  tcb *startPt;
  int max = 0xffff;
  tempPt = RunPt->next;
  startPt = RunPt->next;  
  
	OS_DisableInterrupts();
  do{

    //if thread is not sleeping, then check its pri with max and set it if is more important
    if((tempPt->activePriority < max) && (tempPt->sleepState == 0)){
      max = tempPt->priority;
      NextRunPt = tempPt;
    }   
    tempPt= tempPt->next;

  }while(tempPt != startPt);
	
	
	
	
	NVIC_ST_CURRENT_R = 0;//restart SysTick timer so next thread runs for full timeslice
	if(profileIndex < 100){
	profileData1[profileIndex] = (long *)NextRunPt; //store thread that is starting
	profileData2[profileIndex] = 0x1; //code 1 means new foreground thread is running
	profileData3[profileIndex] = OS_Time();
	profileIndex ++;
	}
	
	NextRunPt->hasNotRun = 0; //indicate that this thread has been run (for aging)
	NextRunPt->activePriority = NextRunPt->priority;
	PendSV_Trigger();
	delay = NVIC_ST_CURRENT_R;
	EnableInterrupts();
}

//****************OS_Sleep********************
//input: time in milliseconds to sleep
//stops execution of the current thread and it will not start again for at least the time given
void OS_Sleep(unsigned short sleepTime){long delay;
	RunPt->sleepState = (sleepTime + 1);
	OS_Suspend();
	delay = RunPt->sleepState;
	
}
//***********OS_Kill****************
//removes the current thread from the TCB linked list and starts running the next thread
//Should not be called if only 1 thread is active
void OS_Kill(void){long i, delay;
	i = OS_StartCritical();
	//remove thread from TCB linked list
	RunPt->prev->next = RunPt->next;
	RunPt->next->prev = RunPt->prev;
	//set next node to head if this node was the head
	if(RunPt == HeadPt){
		HeadPt = RunPt->next;
	}
	//set thread to not in use
	RunPt->free = 1;
	threadCnt --;
	//go to next thread
	OS_EndCritical(i);
	OS_Suspend();
	delay = threadCnt;
	
}

void OS_AddDownTask(void(*task)(void), unsigned char priority){long i;

	i = OS_StartCritical();
	
	  DownTask = task;

		GPIO_PORTE_IS_R &= ~0x02;     // PE1 is edge-sensitive 
	  GPIO_PORTE_IBE_R |= 0x02;    //     PE1 is both edges 
		GPIO_PORTE_ICR_R = 0x02;
		GPIO_PORTE_IM_R |= 0x02;      //  arm interrupt on PE1
	
	  LastPE1 = PE1;
	  NVIC_PRI1_R = (NVIC_PRI1_R&0xFFFFFF00)|((priority&0x07) << 5); // set user priority
	  NVIC_EN0_R |= 0x00000010;              // enable interrupt 4 in NVIC
	
	OS_EndCritical(i);
}

void OS_AddButtonTask(void(*task)(void), int pri){long i;
	i = OS_StartCritical();
	  ButtonTask = task;
		GPIO_PORTF_IS_R &= ~0x02;     // (d) PF1 is edge-sensitive 
		GPIO_PORTF_IBE_R |= 0x02;    //     PF1 is both edges 
		GPIO_PORTF_IM_R |= 0x02;      // (f) arm interrupt on PF1
		GPIO_PORTF_ICR_R = 0x02;
		LastPF1 = PF1;
		NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
		NVIC_EN0_R |= 0x40000000;              // (h) enable interrupt 30 in NVIC
	

	
	OS_EndCritical(i);
}






//*********************OS_Mailbox****************************
//provides funcitons for sending a single piece of data
void OS_MailBox_Send(OSMAILBOXTYPE data){
	OS_bWait(&OSMailboxEmpty);
	mailboxData = data;
	OS_bSignal(&OSMailboxFull);
}

OSMAILBOXTYPE OS_MailBox_Recv(void){OSMAILBOXTYPE retVal;
	OS_bWait(&OSMailboxFull);
	retVal = mailboxData;
	OS_bSignal(&OSMailboxEmpty);
	
	return retVal;
}
	
//lastTime and thisTime are in units of 20ns, period is in  units of 2microsec
//because lastTime and thisTime come from OS_Time which is in the units of 20ns
//Period comes from timer2A, which is in units of 2microseconds.
long Jitter(long lastTime,long thisTime,long period){
long timeDiff;//unit of 20 ns
long jitter;
	 
	timeDiff = OS_TimeDifference(thisTime, lastTime);
	jitter = timeDiff - (period * 20); //multiplying period( which is in units of 2 us) by 20 to get units of 100ns
  return jitter;
}


void OS_DisableInterrupts(void){long i;
	i =  StartCritical();
	if(!(i&0x1)){ //if interrups are actually getting disabed (if I was 0, interrupts were enabled)
		StartCriticalTime = OS_Time();
		StartCriticalValid = 1;
	}
	
}

void OS_EnableInterrupts(void){long endTime, timeDifference, i;
	i = GetInterruptStatus();
	EnableInterrupts();
	if((i&0x01) && StartCriticalValid){ //if interrupts were actually disabled
	endTime = OS_Time();
	StartCriticalValid = 0;
	timeDifference = OS_TimeDifference(endTime, StartCriticalTime)/10;
	if(MaxCritical < timeDifference){
		MaxCritical = timeDifference;
		TotalCritical += timeDifference;
	}
}
	
}

long OS_StartCritical(void){long i;
	i =  StartCritical();
	if(!(i&0x1)){ //if interrups are actually getting disabed (if I was 0, interrupts were enabled)
		StartCriticalTime = OS_Time();
		StartCriticalValid = 1;
	}
	return i;
}

void OS_EndCritical(long i){long endCriticalTime, timeDifference;
	EndCritical(i);
	if(!(i&0x01) && StartCriticalValid){ //if interrupts are actually getting reenabled (if I was 0, interrupts were enabled)
		endCriticalTime = OS_Time();
		StartCriticalValid = 0;
		timeDifference = OS_TimeDifference(endCriticalTime, StartCriticalTime)/10;
		if(MaxCritical<timeDifference){
			MaxCritical = timeDifference;
			TotalCritical += timeDifference;
		}
	}
	
}


void OS_ClearProfiling(void){
	int i = 0;
	
	profileIndex = 0;
	for(i = 0; i < 100; i ++){
		profileData1[i] = 0;
		profileData2[i] = 0;
		profileData3[i] = 0;
	}
	
	
	
}
