#include "hw_types.h"
#include "lm3s8962.h"
#include "sysctl.h"
#include "stdlib.h"
#include "OS_Critical.h"
#include "OS.h"
#include "Debug.h"
#include <string.h>

#if DEBUG == 1
	#include "stdio.h"
	#include "rit128x96x4.h"
  #include "shell.h"
  #include "UART.h"
#endif


static void _OS_Default_Thread(void);

_TCB _threads[_OS_MAX_THREADS]; // static storage for threads
OS_SemaphoreType _modifyTCB;  // binary semaphore for modifying the TCB linked list
_TCB *_RunPt, *_TCBHead;
_OS_FifoType _OS_Fifo;
_OS_MailboxType _OS_Mailbox;
int _OS_numThreads;
void(*_OS_SelTask)(void) = NULL;
void(*_OS_DownTask)(void) = NULL;


/***   Variable declarations   ***/
/* OS system time */
unsigned int _OS_System_Time;
/*** End variable declarations ***/


/* Creates a new semaphore
 * param: int permits, number of permits in semaphore
 *        (permits = 1 -> binary semaphore)
 * returns: allocated, initialized semaphore
 */
void OS_InitSemaphore(OS_SemaphoreType *s, int permits) {
  s->value = permits;
  s->GetIndex = s->PutIndex = 0;
}

// block the currently running thread on a semaphore by setting its blocked flag to true
// and adding it to that semaphore's fifo
void OS_BlockThread(OS_SemaphoreType *s) {
  _TCB *thread = _RunPt;
  thread->block = 1;
  // semaphore fifo guaranteed to have room
  s->blockedThreads[s->PutIndex] = thread;
  s->PutIndex = (s->PutIndex + 1) & (_OS_MAX_THREADS - 1);
  OS_Suspend();
  OS_Delay(OS_ARBITRARY_DELAY);
}

// wake up 1 thread from the semaphore's fifo if there are any waiting
void OS_WakeThread(OS_SemaphoreType *s) {
  if(s->PutIndex != s->GetIndex) {
    s->blockedThreads[s->GetIndex]->block = 0; // wake up thread
    s->GetIndex = (s->GetIndex + 1) & (_OS_MAX_THREADS - 1);
  }
}

// wake up all threads blocked on a semaphore
void OS_WakeAllThreads(OS_SemaphoreType *s) {
  while(s->GetIndex != s->PutIndex) {
    OS_WakeThread(s);
  }
}

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, select switch and timer2 
// input:  none
// output: none
void OS_Init(void) {
  int i;
	DisableInterrupts();
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_8MHZ | SYSCTL_OSC_MAIN);
  /* Initialize SysTick */
	NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
	/* Initialize PendSV */
	NVIC_INT_CTRL_R = 0;				// disable PendSV during setup, may not be necessary, or my be vital ?!?!
	NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0xFF00FFFF)|0x00E00000; // lowest priority
	/* Initialize foreground linked list */
  OS_InitSemaphore(&_modifyTCB, OS_BINARY_SEMAPHORE);
  _RunPt = NULL;
  _TCBHead = NULL;
  // intitalize array of threads to indicate they are all free
  for(i = 0; i < _OS_MAX_THREADS; i++) {
    _threads[i].id = _OS_FREE_THREAD;
  }
  _OS_numThreads = 0;
  
  // initialize timers used by OS
  Timer2A_Init();
  Timer2B_Init(0);
  
  /* Add default thread in case all threads killed */
  OS_AddThread(&_OS_Default_Thread, 0, 7); // should be lowest priority
}

// return the id of the thread pointed to by _RunPt
// if _RunPt is null, return -1
int OS_Id(void) {
  if(_RunPt == NULL) {
    return -1;
  }
  return _RunPt->id;
}

///******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 20ns clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(unsigned long theTimeSlice) {
  NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  NVIC_ST_CTRL_R = 0x00000007; // enable, core clock and interrupt arm
  StartOS();
}

static void _OS_Default_Thread(void) {
  while(1)
    OS_Suspend();
}

// OS_Delay
// count up to specified value
// should be used after calls to OS_Kill, Suspend, etc
void OS_Delay(int count) {
  int i;
  for(i = 0; i < count; i++)
    ;
}
