#include "OS.h"
#include "hw_types.h"
#include "lm3s8962.h"
#include <stdlib.h>

#define _OS_InitThread(THREAD,TASK,NEXT,PREV,ID,SLEEP,BLOCK,PRIORITY)   \
  _OS_SetInitialStack(THREAD, TASK);                                           \
  THREAD->next = NEXT;                                                         \
  THREAD->prev = PREV;                                                         \
  THREAD->id = ID;                                                             \
  THREAD->sleep = SLEEP;                                                       \
  THREAD->block = BLOCK;                                                       \
  THREAD->priority = PRIORITY;
  
extern _TCB _threads[_OS_MAX_THREADS]; // static storage for threads
extern OS_SemaphoreType _modifyTCB;  // binary semaphore for modifying the TCB linked list
extern _TCB *_RunPt;
extern int _OS_numThreads;

static void _OS_SetInitialStack(_TCB* thread, void(*task)(void));

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority) {
  _TCB* thread;// = (_TCB*) malloc(sizeof(_TCB));
  int i, tid = _OS_numThreads++;
  // lock thread linked list
//  OS_bWait(&_modifyTCB);
  if(_RunPt == NULL) {
    // this is the first thread
    thread = &_threads[0];
    _OS_InitThread(thread, task, thread, thread, tid, 0, 0, priority) // TODO: deide how to handle stacks
    _RunPt = thread;
  }
  else {
    // need to find the first open space in thread storage array
    for(i = 0; i < _OS_MAX_THREADS; i++) {
      if(_threads[i].id == _OS_FREE_THREAD) {
        break;
      }
    }
    if(i == _OS_MAX_THREADS) {
      // maximum number of threads already used
      // TODO - handle this more elegantly
      //OS_bSignal(&_modifyTCB);
      return 0;
    }
    else {
      _TCB* temp = _RunPt;  // store _RunPt in case thread switcher changes it while this is happening
      thread = &_threads[i];
      // add thread between RunPt.previous and RunPt, at the end of the list
      _OS_InitThread(thread, task, temp, temp->prev, tid, 0, 0, priority)
      temp->prev->next = thread;
      temp->prev = thread;
    }
  }
  // unlock thread linked list
//  OS_bSignal(&_modifyTCB);
  return 1;
}

static void _OS_SetInitialStack(_TCB* thread, void(*task)(void)){
//  thread->stack = (unsigned long*) malloc(_OS_STACK_SIZE * sizeof(unsigned long));
  thread->sp = &thread->stack[_OS_STACK_SIZE-16]; // thread stack pointer
  thread->stack[_OS_STACK_SIZE-1] = 0x01000000;   // thumb bit
  thread->stack[_OS_STACK_SIZE-2] = (unsigned long) task; // initial pc
  thread->stack[_OS_STACK_SIZE-3] = 0x14141414;   // R14
  thread->stack[_OS_STACK_SIZE-4] = 0x12121212;   // R12
  thread->stack[_OS_STACK_SIZE-5] = 0x03030303;   // R3
  thread->stack[_OS_STACK_SIZE-6] = 0x02020202;   // R2
  thread->stack[_OS_STACK_SIZE-7] = 0x01010101;   // R1
  thread->stack[_OS_STACK_SIZE-8] = 0x00000000;   // R0
  thread->stack[_OS_STACK_SIZE-9] = 0x11111111;   // R11
  thread->stack[_OS_STACK_SIZE-10] = 0x10101010;  // R10
  thread->stack[_OS_STACK_SIZE-11] = 0x09090909;  // R9
  thread->stack[_OS_STACK_SIZE-12] = 0x08080808;  // R8
  thread->stack[_OS_STACK_SIZE-13] = 0x07070707;  // R7
  thread->stack[_OS_STACK_SIZE-14] = 0x06060606;  // R6
  thread->stack[_OS_STACK_SIZE-15] = 0x05050505;  // R5
  thread->stack[_OS_STACK_SIZE-16] = 0x04040404;  // R4
}

/* Remove _RunPt from the linked list */
void OS_Kill(void)
{
  int i;
	_TCB *temp = _RunPt;
	OS_bWait(&_modifyTCB);
  // remove thread from linked list
	temp->prev->next = temp->next;
	temp->next->prev = temp->prev;
	// search tcb array to find it by checking unique id's
  for(i = 0; i < _OS_MAX_THREADS; i++) {
    if(_threads[i].id == temp->id) {
      _threads[i].id = _OS_FREE_THREAD; // delete thread (symbolically)
      break;
    }
  }
  if(i == _OS_MAX_THREADS) {
      // TODO - this should never happen
  }
	OS_bSignal(&_modifyTCB);
  NVIC_ST_CURRENT_R = 0x0; // any write clears
  NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PENDSTSET; // trigger systick interrupt
  OS_Delay(OS_ARBITRARY_DELAY);
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void) {
  NVIC_ST_CURRENT_R = 0x0; // any write clears
  NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PENDSTSET;  // trigger a systick interrupt to switch threads
  OS_Delay(OS_ARBITRARY_DELAY);
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime) {
  _RunPt->sleepTime = sleepTime;  // how long to sleep
  _RunPt->sleep = 1;  // indicate this thread is now sleeping
  OS_Suspend(); // trigger thread switch
}

// during a context switch, index _RunPt to the first non-sleeping/blocked thread
void OS_FindNextThread(void)
{
  _RunPt = _RunPt->next;  // assume RunPt points to the thread that just finished running
  while(_RunPt->sleep || _RunPt->block) {
    _RunPt = _RunPt->next;
  }
}
