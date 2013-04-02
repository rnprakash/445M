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

#define _OS_InitThread(THREAD,TASK,NEXT,PREV,ID,SLEEP,BLOCK,PRIORITY)   \
  _OS_SetInitialStack(THREAD, TASK);                                           \
  THREAD->next = NEXT;                                                         \
  THREAD->prev = PREV;                                                         \
  THREAD->id = ID;                                                             \
  THREAD->sleep = SLEEP;                                                       \
  THREAD->block = BLOCK;                                                       \
  THREAD->priority = PRIORITY;

/* static */ _TCB _threads[_OS_MAX_THREADS]; // static storage for threads
static OS_SemaphoreType _modifyTCB;  // binary semaphore for modifying the TCB linked list
_TCB *_RunPt;
static _OS_FifoType _OS_Fifo;
static _OS_MailboxType _OS_Mailbox;
static int _OS_numThreads;
static void(*_OS_SelTask)(void) = NULL;
static void(*_OS_DownTask)(void) = NULL;

/***   Function declarations   ***/
static void _OS_Inc_Time(void);
static void _OS_Update_Root(_OS_Task * temp, _OS_Task * cur_task);
// Initialize a thread
// takes as arguments the thread to initialize and a value for each field in the thread
//static void _OS_InitThread(_TCB* thread, void(*task)(void), _TCB* next, _TCB* prev, int id, char sleep, char block, unsigned long priority);
static void _OS_SetInitialStack(_TCB* thread, void(*task)(void));
// print information from n threads, starting with RunPt and cycling if necessary
static void _OS_PrintThreads(int n);
static void _OS_PrintThread(_TCB* thread);
static void _OS_Default_Thread(void);
static void DebouncePortFTask(void);
static void DebouncePortETask(void);
/*** End function declarations ***/


/***   Variable declarations   ***/
/* Linked list of tasks */
static _OS_Task* _OS_Root = NULL;
/* Interrupt counter, tasks executed based on this */
static unsigned int _OS_Task_Time = 0;
/* OS system time */
static unsigned int _OS_System_Time;
/*** End variable declarations ***/


/* Initialize timer2 for system time */
void Timer2A_Init(void)
{
	int nop = 5;
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2;	/* Activate timer2A */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	TIMER2_CTL_R &= ~0x00000001;						/* Disable timer2A during setup */
	TIMER2_CFG_R = 0x00000004;							/* Configure for 16-bit timer mode */
	TIMER2_TAMR_R = 0x00000002;							/* Configure for periodic mode */
	TIMER2_TAPR_R = 49;											/* 1us timer2A */
	TIMER2_ICR_R |= 0x00000001;							/* Clear timer2A timeout flag */
	TIMER2_TAILR_R = 100;										/* Reload time of 100us */
	TIMER2_IMR_R |= TIMER_IMR_TATOIM;				/* Arm timeout interrupt */
	
	/* Add system time task */
	OS_Add_Periodic_Thread(&_OS_Inc_Time, 1, 2);
}

// millisecond timer used to keep track of / wake up sleeping threads
void Timer2B_Init(int priority) {
  int nop = 5;
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2;	/* Activate timer2 */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	TIMER2_CTL_R &= ~0x00000100;						/* Disable timer2B during setup */
	TIMER2_CFG_R = 0x00000004;							/* Configure for 16-bit timer mode */
	TIMER2_TBMR_R = 0x00000002;							/* Configure for periodic mode */
	TIMER2_TBPR_R = 1;											/* 40 ns timer2B */
	TIMER2_ICR_R |= 0x00000100;							/* Clear timer2B timeout flag */
	TIMER2_TBILR_R |= 0x0000FFFF;									/* Reload time of 2.6ms */
  NVIC_EN0_R |= NVIC_EN0_INT24;           /* enable NVIC */
  NVIC_PRI6_R = ((NVIC_PRI6_R & 0xFFFFFFFF) | (priority << 5)); /* set priority for ms time */
  TIMER2_CTL_R |= 0x00000100;           /* enable timer2B */
	TIMER2_IMR_R |= TIMER_IMR_TBTOIM;				/* Arm timeout interrupt */
}

/* Adds a new task to the Timer2A interrupt thread scheduler
 * param: void (*task)(void), function pointer of task to be called
 * param: unsigned long period, period of task
 * param: unsigned long priority, priority of task
 * return: 0 if task successfully added to thread scheduler,
					 1 if maximum threads already queued.
 */
int OS_Add_Periodic_Thread(void (*task)(void), unsigned long period, unsigned long priority)
{
	/* Declare function as critical */
	OS_CRITICAL_FUNCTION;
	
	/* Number of scheduled tasks */
	static int _OS_Num_Tasks = 0;
	
	/* Allocate variables */
	_OS_Task *new_task;
	int new_priority;

	/* Bounds checking */
	if(_OS_Num_Tasks >= OS_MAX_TASKS)
		return -1;

	/* Allocate space for new task */
	new_task = (_OS_Task*)malloc(sizeof(_OS_Task) + 1);
	
	/* Set task parameters */
	new_task->task = task;
	new_task->period = period * 10;
	new_task->priority = priority;
	new_task->task_id = _OS_Num_Tasks;
	new_task->time = period * 10;	
	new_task->next = NULL;							/* No next task (last task in list) */
	
	/* Calculate outside critical section */
	new_priority = ((NVIC_PRI5_R&0x00FFFFFF)
									| (1 << (28 + priority)));
	
	/* Start critical section */
	OS_ENTER_CRITICAL();

	/* Insert task into position  */
	if(_OS_Root == NULL)
		_OS_Root = new_task;
	else
		new_task->next = _OS_Root, _OS_Root = new_task;
	_OS_Update_Root(_OS_Root, new_task);

	/* Update interrupt values */
	NVIC_PRI5_R = new_priority;
	
	/* Enable timer and interrupt (in case this is the first task) */
	TIMER2_CTL_R |= 0x00000001;
	NVIC_EN0_R |= NVIC_EN0_INT23;
	/* End critical section */
	OS_EXIT_CRITICAL();
	return _OS_Num_Tasks++;
}

/* Execute next periodic task if interrupt count and
		task time match. Update task list and next interrupt
		priority if task is executed.
 * param: none
 * return: none
 */
/*static*/ unsigned long _us100Count = 0;
void Timer2A_Handler(void)
{
  int i;
	/* Declare function as critical */
//	OS_CRITICAL_FUNCTION;
	
	_OS_Task *cur_task = _OS_Root;
	
	/* Acknowledge interrupt */
	TIMER2_ICR_R = TIMER_ICR_TATOCINT;
  
  // Sleep maitenance
  _us100Count++;
  if((_us100Count % 10) == 0) { // 1 millisecond elapsed
    // find all sleeping threads and decrement their ms time,
    // and wake up if ready
    for(i = 0; i < _OS_MAX_THREADS; i++) {
      if((_threads[i].id != _OS_FREE_THREAD) && _threads[i].sleep) {
        if(_threads[i].sleepTime == 0) {
          _threads[i].sleep = 0;  // wake up thread
        }
        else {
          _threads[i].sleepTime--;
        }
      }
    }
  }
  
	/* Update task time */
	_OS_Task_Time++;
	
	if(_OS_Task_Time <= _OS_Root->time)
		return;
	
	/* Execute task */
	cur_task->task();
	
	/* Begin critical section */
//	OS_ENTER_CRITICAL();
	
	/* Update task's time */
	cur_task->time += cur_task->period;
	
	/* Insert executed task into new position */
	_OS_Update_Root(_OS_Root, cur_task);
	
	/* Update interrupt priority */
	NVIC_PRI5_R = _OS_Root->priority;
	
	/* End critical section */
//	OS_EXIT_CRITICAL();
}

void Timer2B_Handler(void) {
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;  // acknowledge
}

static void _OS_Update_Root(_OS_Task *temp, _OS_Task * cur_task)
{
	/* Find new position */
	while(temp->next && temp->next->time < cur_task->time)
		temp = temp->next;
	
	/* Create links and update OS root if necessary */
	if(temp != cur_task)
	{
		_OS_Root = _OS_Root->next;
		cur_task->next = temp->next;
		temp->next = cur_task;
	}
}

/* Increment OS system time (in milliseconds)
 * param: none
 * return: none
 */
static void _OS_Inc_Time()
{
	_OS_System_Time++;
}

/* Return OS system time (in milliseconds)
 * param: none
 * return: OS time in milliseconds
 */
unsigned int OS_MsTime(void)
{
	return _OS_System_Time;
}

/* Clears the OS system time
 * param: none
 * return: none
 */
void OS_ClearMsTime(void)
{
	_OS_System_Time = 0;
}

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
      OS_bSignal(&_modifyTCB);
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
  // intitalize array of threads to indicate they are all free
  for(i = 0; i < _OS_MAX_THREADS; i++) {
    _threads[i].id = _OS_FREE_THREAD;
  }
  _OS_numThreads = 0;
  
  // initialize timers used by OS
  Timer2A_Init();
  Timer2B_Init(2);
  
  /* Add default thread in case all threads killed */
//  OS_AddThread(&_OS_Default_Thread, 0, 0); // should be lowest priority
}

// return the id of the thread pointed to by _RunPt
// if _RunPt is null, return -1
int OS_Id(void) {
  if(_RunPt == NULL) {
    return -1;
  }
  return _RunPt->id;
}

// redefine for debugging that needs access to private variables
void OS_Debug(void) {
  UART_OutString("threads:\r\n");
  _OS_PrintThreads(3);
}

// print information from n threads, starting with RunPt and cycling if necessary
static void _OS_PrintThreads(int n) {
  int i;
  _TCB *temp;
  if (_RunPt != NULL) {
    temp = _RunPt;
    for(i = 0; i < n; i++) {
      _OS_PrintThread(temp);
      temp = temp->next;
    }      
  }
  else {
    UART_OutString("No threads"); UART_OutString(SH_NL);
  }
}

static void _OS_PrintThread(_TCB* thread) {
  UART_OutString("this = "); UART_OutUDec(thread->id); UART_OutString(SH_NL);
  UART_OutString("next = "); UART_OutUDec(thread->next->id); UART_OutString(SH_NL);
  UART_OutString("prev = "); UART_OutUDec(thread->prev->id); UART_OutString(SH_NL);
  UART_OutString("priority = "); UART_OutUDec(thread->priority); UART_OutString(SH_NL);
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

static void _OS_Default_Thread(void) {
  while(1)
    OS_Suspend();
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

// OS_Delay
// count up to specified value
// should be used after calls to OS_Kill, Suspend, etc
void OS_Delay(int count) {
  int i;
  for(i = 0; i < count; i++)
    ;
}

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
int OS_AddButtonTask(void(*task)(void), unsigned long priority) {
  static unsigned int haveInit = 0;  // only initialize once
  if(!haveInit) {
    PORTF_Init();  // initialize; for now, just select switch (PF1)
    haveInit = 1;
  }
  _OS_SelTask = task;
  // initialize NVIC interrupts for port F
  NVIC_PRI7_R = ((NVIC_PRI7_R&0xFF0FFFFFFF)
									| (priority << 21));
  NVIC_EN0_R |= NVIC_EN0_INT30;
  return 1;  
}

// the same functionality as AddButtonTask, except for port E
int OS_AddDownTask(void(*task)(void), unsigned long priority) {
  static unsigned int haveInit = 0;  // only initialize once
  if(!haveInit) {
    PORTE_Init();  // initialize; for now, just down switch (PE1)
    haveInit = 1;
  }
  _OS_DownTask = task;
  // initialize NVIC interrupts for port E
  NVIC_PRI1_R = ((NVIC_PRI1_R&0xFFFFFFFF0F)
									| (priority << 5));
  NVIC_EN0_R |= NVIC_EN0_INT4;
  return 1;  
}

//static unsigned long LastPF1 = 1;
void GPIOPortF_Handler(void) {
  if(_OS_SelTask != NULL) {
    OS_AddThread(_OS_SelTask, _OS_STACK_SIZE, 1);
  }
  GPIO_PORTF_IM_R &= ~PORTF_PINS; // disarm interrupt
  OS_AddThread(&DebouncePortFTask, _OS_STACK_SIZE, 5); // TODO - handle priority
}

void GPIOPortE_Handler(void) {
  if(_OS_DownTask != NULL) {
    OS_AddThread(_OS_DownTask, _OS_STACK_SIZE, 1);
  }
  GPIO_PORTE_IM_R &= ~PORTE_PINS; // disarm interrupt
  OS_AddThread(&DebouncePortETask, _OS_STACK_SIZE, 5); // TODO - handle priority
}

static void DebouncePortFTask(void) {
  OS_Sleep(BUTTON_SLEEP_MS);   // foreground sleeping, must run within 50ms
  GPIO_PORTF_ICR_R |= PORTF_PINS;    // acknowledge interrupt
  GPIO_PORTF_IM_R |= PORTF_PINS;    // re-arm interrupt
  OS_Kill();
  OS_Delay(OS_ARBITRARY_DELAY);
}

static void DebouncePortETask(void) {
  OS_Sleep(BUTTON_SLEEP_MS);   // foreground sleeping, must run within 50ms
  GPIO_PORTE_ICR_R |= PORTE_PINS;    // acknowledge interrupt
  GPIO_PORTE_IM_R |= PORTE_PINS;    // re-arm interrupt
  OS_Kill();
  OS_Delay(OS_ARBITRARY_DELAY);
}

void PORTF_Init(void) {
  volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF;
  delay = SYSCTL_RCGC2_R;
  GPIO_PORTF_IM_R &= ~PORTF_PINS;  // interrupt mask register should be set to 0 for setup
  GPIO_PORTF_DIR_R &= ~PORTF_PINS; // input
  GPIO_PORTF_DEN_R |= PORTF_PINS;  // digital mode
  GPIO_PORTF_PUR_R |= PORTF_PINS;  // enable pull-up res
  GPIO_PORTF_IS_R &= ~PORTF_PINS;  // edge-sensitive
  GPIO_PORTF_IBE_R &= ~PORTF_PINS;  // interrupt both edges
  GPIO_PORTF_ICR_R = PORTF_PINS;   // clear flags
  GPIO_PORTF_IM_R |= PORTF_PINS;   // re-arm interrupt
}

void PORTE_Init(void) {
  volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE;
  delay = SYSCTL_RCGC2_R;
  GPIO_PORTE_IM_R &= ~PORTE_PINS;  // interrupt mask register should be set to 0 for setup
  GPIO_PORTE_DIR_R &= ~PORTE_PINS; // input
  GPIO_PORTE_DEN_R |= PORTE_PINS;  // digital mode
  GPIO_PORTE_PUR_R |= PORTE_PINS;  // enable pull-up res
  GPIO_PORTE_IS_R &= ~PORTE_PINS;  // edge-sensitive
  GPIO_PORTE_IBE_R &= ~PORTE_PINS;  // not interrupt both edges
//  GPIO_PORTE_IEV_R &= ~PORTE_PINS;   // rising edge triggered
  GPIO_PORTE_ICR_R = PORTE_PINS;   // clear flags
  GPIO_PORTE_IM_R |= PORTE_PINS;   // re-arm interrupt
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
void OS_FindNextThread(void) {
  // is this safe?
  // if the operating system enters a state where all threads are blocked or sleeping,
  // this could loop indefinitely
	// dummy thread never sleeps or blocks.
  _RunPt = _RunPt->next;  // assume _RunPt is pointing at the thread that just run, start search from the next thread
  while(_RunPt->sleep || _RunPt->block) {
    _RunPt = _RunPt->next;
  }
}

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size) {
//  memset(_OS_Fifo.Fifo, 0, sizeof(_OS_Fifo.Fifo));  // initialize all values to 0
  _OS_Fifo.PutIndex = _OS_Fifo.GetIndex = 0;
  OS_InitSemaphore(&_OS_Fifo.notEmpty, 0);
  OS_InitSemaphore(&_OS_Fifo.mutex, OS_BINARY_SEMAPHORE);
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data) {
  // NOT THREAD SAFE!!
  // doesn't this actually test if there's only 1 spot left?
  if((_OS_Fifo.PutIndex == _OS_Fifo.GetIndex) && (OS_Fifo_Size() > 0)) {
    return 0;
  }
  _OS_Fifo.Fifo[_OS_Fifo.PutIndex] = data;
  _OS_Fifo.PutIndex = (_OS_Fifo.PutIndex + 1) & (_OS_FIFO_SIZE - 1);
  OS_Signal(&_OS_Fifo.notEmpty);
  return 1;
}  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void) {
  unsigned long data;
  OS_Wait(&_OS_Fifo.notEmpty);
  OS_bWait(&_OS_Fifo.mutex);
  data = _OS_Fifo.Fifo[_OS_Fifo.GetIndex];
  _OS_Fifo.GetIndex = (_OS_Fifo.GetIndex + 1) & (_OS_FIFO_SIZE - 1);
  OS_bSignal(&_OS_Fifo.mutex);
  return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero  if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void) {
  return (_OS_Fifo.PutIndex - _OS_Fifo.GetIndex) & (_OS_FIFO_SIZE - 1);
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void) {
  _OS_Mailbox.data = 0;
   OS_InitSemaphore(&_OS_Mailbox.hasData, 0);
   OS_InitSemaphore(&_OS_Mailbox.gotData, 1);
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data) {
  OS_bWait(&_OS_Mailbox.gotData);
  _OS_Mailbox.data = data;
  OS_bSignal(&_OS_Mailbox.hasData);  
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void) {
  unsigned long data;
  OS_bWait(&_OS_Mailbox.hasData);
  data = _OS_Mailbox.data;
  OS_bSignal(&_OS_Mailbox.gotData);
  return data;
}

// ******** OS_Time ************
// reads a timer value 
// Inputs:  none
// Outputs: time in 20ns units, 0 to max
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void) {
  return /*NVIC_ST_CURRENT_R;*/TIMER2_TBR_R * 2;//_us10Count * 10;  // 1us = 20ns * 50
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 20ns units 
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop) {
  if(stop > start) {
    return (stop - start);  // inputs should already be in 20ns units
  }
  return start - stop;
}

