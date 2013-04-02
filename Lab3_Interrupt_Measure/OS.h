#ifndef __OS_H__
#define __OS_H__

#define OS_MAX_TASKS 4
#define OS_BINARY_SEMAPHORE 1
#define _OS_STACK_SIZE 128 // stack size in words (long)
#define _OS_MAX_THREADS 64
#define _OS_FREE_THREAD -1
#define OS_ARBITRARY_DELAY 100
#define _OS_FIFO_SIZE 32  // must be power of 2
#define MAX_FIFO_SIZE _OS_FIFO_SIZE
#define PF1 (*(volatile unsigned long *)0x40025000 + 0x80)
#define PORTF_PINS 0x02 // select button, PF1
#define PORTE_PINS 0x02 // down button, PE1
#define TIME_1MS  50000          
#define TIME_2MS  2*TIME_1MS
#define BUTTON_SLEEP_MS 400
#define JITTERSIZE 64
#define _OS_MAX_EVENTS 100
#define EVENT_FIFO_PUT 0
#define EVENT_FIFO_GET 1
#define EVENT_FIFO_WAIT 2
#define EVENT_FIFO_WAKE 3
#define EVENT_CONSUMER_RUN 4
#define EVENT_CONSUMER_GOT 5
#define EVENT_OLED_START 6
#define EVENT_OLED_FINISH 7
#define EVENT_THREAD 32

#include "OS_types.h"

void Timer2A_Init(void);
int OS_Add_Periodic_Thread(void(*task)(void), unsigned long period, unsigned long priority);
void Timer2A_Handler(void);
void Timer2B_Handler(void);
unsigned int OS_MsTime(void);
void OS_ClearMsTime(void);

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, select switch and timer2 
// input:  none
// output: none
void OS_Init(void); 

/* Creates a new semaphore
 * param: int permits, number of permits in semaphore
 *        (permits = 1 -> binary semaphore)
 * returns: allocated, initialized semaphore
 */
void OS_InitSemaphore(OS_SemaphoreType *s, int permits);

// block a thread on a semaphore by setting its blocked flag to true
// and adding it to that semaphore's fifo
void OS_BlockThread(OS_SemaphoreType *s);

// wake up 1 thread from the semaphore's fifo if there are any waiting
void OS_WakeThread(OS_SemaphoreType *s);

// wake up all threads blocked on a semaphore
void OS_WakeAllThreads(OS_SemaphoreType *s);

// ******** OS_Wait ************
// decrement semaphore and spin/block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(OS_SemaphoreType* s);

// ******** OS_Signal ************
// increment semaphore, wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(OS_SemaphoreType* s);

// ******** OS_bWait ************
// if the semaphore is 0 then spin/block
// if the semaphore is 1, then clear semaphore to 0
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(OS_SemaphoreType* s);

// ******** OS_bSignal ************
// set semaphore to 1, wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(OS_SemaphoreType* s);


// StartOS
void StartOS(void);

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority);

///******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 20ns clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(unsigned long theTimeSlice);

//******** OS_Id *************** 
// return the id of the thread pointed to by _RunPt
// if _RunPt is null, return -1
int OS_Id(void);

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
int OS_AddButtonTask(void(*task)(void), unsigned long priority);

// the same functionality as AddButtonTask, except for port E
int OS_AddDownTask(void(*task)(void), unsigned long priority);

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime);

// ******** OS_Kill ************
// kill the currently running thread, release its TCB memory
// input:  none
// output: none
void OS_Kill(void);

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void);

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size);

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data);  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void);

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero  if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void);

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void);

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data);

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void);

// ******** OS_Time ************
// reads a timer value 
// Inputs:  none
// Outputs: time in 20ns units, 0 to max
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void);

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 20ns units 
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
long OS_TimeDifference(unsigned long start, unsigned long stop);


// OS_Delay
// count up to specified value
// should be used after calls to OS_Kill, Suspend, etc
void OS_Delay(int count);

// redefine for debugging that needs access to private variables
void OS_Debug(void);

void PORTF_Init(void);

void PORTE_Init(void);

// millisecond timer used to keep track of / wake up sleeping threads
void Timer2B_Init(int priority);

// during a context switch, index _RunPt to the first non-sleeping/blocked thread
void OS_FindNextThread(void);

// measure & record the jitter for a given periodic task
// period should be in units of XXXXXXX - TODO
void OS_MeasureJitter(int taskID, unsigned long period);
long OS_getMaxJitter(int taskID);
long OS_getMinJitter(int taskID);

// Insert a thread into it's proper position in the LL
// either because it's priority has changed or it has run
// much easier if LL is doubly linked
void _OS_InsertThread(_TCB* thread);

// remove a thread from the LL
void _OS_RemoveFromLL(_TCB *thread);

void OS_start_interrupt(void);
void OS_end_interrupt(void);
unsigned long OS_max_int_time(void);
unsigned long OS_total_int_percentage(void);

// log an event
// returns 1 if successful, 0 otherwise
int OS_LogEvent(char type);

void DisableInterrupts(void);
void EnableInterrupts(void);

#endif
