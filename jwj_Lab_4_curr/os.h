// OS.h
// Runs on LM3S1968
// Use Timer0A in periodic mode to request interrupts at a particular
// period.
// Daniel Valvano
// September 14, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
  Program 7.5, example 7.6
8
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
 
#ifndef __OS_H
#define __OS_H  1

// fill these depending on your clock        
#define SYSTICK_TIME_1MS  50000 //in units of 
#define TIME_1MS 10000        //if each tick is 100ns (OS_time, systick) 
#define TIME_2MS  2*OS_TIME_1MS   
#define PERIODIC_TIME_1MS 500  //in units of 2 micro seconds
 
 
#define PD2 (*((volatile unsigned long *)0x40007010))
#define PD3 (*((volatile unsigned long *)0x40007020))
#define PD4 (*((volatile unsigned long *)0x40007040))
#define PD5 (*((volatile unsigned long *)0x40007080))
#define PD6 (*((volatile unsigned long *)0x40007100))
#define PD7 (*((volatile unsigned long *)0x40007200))

struct tcbtype{
	long *sp;
	struct tcbtype *next;
	struct tcbtype *prev;
	long sleepState;
	long priority;
	long activePriority;
	long hasNotRun;
	char id;
	int free;
};
typedef struct tcbtype tcb;


struct sema4struct{
	volatile long value; //semaphore value
	volatile long size;
	tcb *HeadPt; //linked list of blocked threads
	tcb *TailPt; 
};
 
typedef struct sema4struct Sema4Type;

#define JITTERSIZE 64
struct bckgndTcbType{
  void (*task)(void);
	struct bckgndTcbType *next;
	unsigned short period;
	long timeTilExec;
	long lastWaitTime;
	int priority; 
	long lastTime;
	long currTime;
	long MinJitter;
	long MaxJitter;
  unsigned long JitterHistogram[JITTERSIZE];

};

typedef struct bckgndTcbType bckgndTcb;





typedef unsigned short OSFIFOTYPE;
typedef unsigned long OSMAILBOXTYPE;

extern Sema4Type OLEDFree;
extern Sema4Type TxFIFOSpaceAvail;
extern Sema4Type RxFIFOSpaceAvail;
extern Sema4Type RxFIFODataAvail;
extern Sema4Type TxFIFODataAvail;
extern Sema4Type TxFIFOFree;
extern Sema4Type RxFIFOFree;
extern Sema4Type OSFIFODataAvail;
extern Sema4Type OSFIFOSpaceAvail;
extern Sema4Type OSFIFOFree;
extern Sema4Type OSMailboxFull;
extern Sema4Type OSMailboxEmpty;
extern long MaxJitter;    // largest time difference between interrupt trigger and running thread
extern long MinJitter;    // smallest time difference between interrupt trigger and running thread

extern long *profileData1[100];
extern unsigned char profileData2[100];
extern long profileData3[100];



// ***************** Timer2A_Init ****************
// Activate Timer2A interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in msec
// Outputs: none
void Timer2A_Init(void(*task)(void), unsigned short period);

//functions to access global counter, which is incrememnted every period given in Timer2A_Init
void OS_CLearMsTime(void);
unsigned long OS_MsTime(void);


//******************Semaphor access functions
//Wait and Signal are for counting semaphores
//bWait and bSignal are for binary semaphores
void OS_Wait(Sema4Type *s);

void OS_SpinWait(Sema4Type *s);

void OS_Signal(Sema4Type *s);

void OS_SpinSignal(Sema4Type *s);

void OS_bWait(Sema4Type *s);

void OS_bSpinWait(Sema4Type *s);

void OS_bSignal (Sema4Type *s);

void OS_bSpinSignal (Sema4Type *s);

void OS_Init(void);

void OS_InitSemaphore(Sema4Type *s, int initalValue);

int OS_AddThread(void(*task)(void), int stackSize, int arg3);

char OS_Id(void);

void OS_Launch(unsigned long theTimeSlice);

void OS_Suspend(void);

void OS_Kill(void);

void OS_Sleep(unsigned short timems);
void OS_AddDownTask(void(*task)(void), unsigned char priority);
//return current clock time. Returns a long, but the value is only 16 bits because a 16 bit timer was used
long OS_Time(void);

//returns the difference between newTime and oldTime (they should both only be 16 bits)
long OS_TimeDifference(long newTime, long oldTime);

//returns the maximum time the OS was running with interrupts disabled in units of us
unsigned long OS_MaxCritical(void);

// returns the percentage of time the OS was running without interrupts in units of 1/10th of a percent
unsigned long OS_PercentCritical(void);

//clears all data relating to time in critical sections
void OS_ClearCriticalStats(void);

// adds a given task to the timer2A interrupt, so it is run in the background every given period
bckgndTcb* OS_AddPeriodicThread(void(*task)(void), unsigned short period, int arg3);

void OS_AddButtonTask(void(*task)(void), int arg2);


//***********OS_FIFO*****************
//Provides functions to access a semaphored FIFO for interthread communication
int OS_Fifo_Put(OSFIFOTYPE data);

int OS_Fifo_Get(OSFIFOTYPE *data);

//*********************OS_Mailbox****************************
//provides funcitons for sending a single piece of data
void OS_MailBox_Send(OSMAILBOXTYPE data);

OSMAILBOXTYPE OS_MailBox_Recv(void);

long Jitter(long lastTime, long thisTime, long period);



//*************************OS_Interrupt functions******************
void OS_DisableInterrupts(void);

void OS_EnableInterrupts(void);

long OS_StartCritical(void);

void OS_EndCritical(long i);



void OS_ClearProfiling(void);
#endif

