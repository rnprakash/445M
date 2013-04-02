#include "hw_types.h"
#include "lm3s8962.h"
#include "sysctl.h"
#include "stdlib.h"
#include "OS_Critical.h"
#include "OS.h"
#include "Debug.h"

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

/* Task node for linked list */
typedef struct _OS_Task {
	void (*task)(void);			/* Periodic task to perform */
	unsigned long time,			/* _OS_Task_Time at which to perform */
							priority,		/* Task priority */
							period,			/* Frequency in units of 100ns */
							task_id;		/* Task id */
	struct _OS_Task *next;	/* Pointer to next task to perform*/
} _OS_Task;

/* Thread struct */
typedef struct _TCB {
  volatile unsigned long * sp;     /* stack pointer */
	struct _TCB * next, * prev;			/* Link pointers */
	int id;													/* Thread id */
	char sleep, block;							/* Flags for sleep and block states */
	unsigned long priority;					/* Thread priority */
	volatile unsigned long stack[_OS_STACK_SIZE];	/* Pointer to thread's stack */
} _TCB;

static OS_SemaphoreType* _modifyTCB;  // binary semaphore for modifying the TCB linked list
_TCB *_RunPt;
static int _OS_numThreads;


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
	TIMER2_ICR_R = 0x00000001;							/* Clear timer2A timeout flag */
	TIMER2_TAILR_R = 100;										/* Reload time of 100us */
	TIMER2_IMR_R |= TIMER_IMR_TATOIM;				/* Arm timeout interrupt */
	
	/* Add system time task */
	OS_Add_Periodic_Thread(&_OS_Inc_Time, 1, 4);
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
void Timer2A_Handler(void)
{
	/* Declare function as critical */
	OS_CRITICAL_FUNCTION;
	
	_OS_Task *cur_task = _OS_Root;
	
	/* Acknowledge interrupt */
	TIMER2_ICR_R = TIMER_ICR_TATOCINT;
  
	/* Update task time */
	_OS_Task_Time++;
	
	if(_OS_Task_Time <= _OS_Root->time)
		return;
	
	/* Execute task */
	cur_task->task();
	
	/* Begin critical section */
	OS_ENTER_CRITICAL();
	
	/* Update task's time */
	cur_task->time += cur_task->period;
	
	/* Insert executed task into new position */
	_OS_Update_Root(_OS_Root, cur_task);
	
	/* Update interrupt priority */
	NVIC_PRI5_R = _OS_Root->priority;
	
	/* End critical section */
	OS_EXIT_CRITICAL();
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
OS_SemaphoreType* OS_InitSemaphore(int permits) {
  OS_SemaphoreType* s = (OS_SemaphoreType*) malloc(sizeof(int));
  s->value = permits;
  return s;
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
  _TCB *thread = (_TCB*) malloc(sizeof(_TCB));
  int tid = _OS_numThreads++;
  // lock thread linked list
  OS_bWait(_modifyTCB);
  if(_RunPt == NULL) {
    // this is the first thread
    _OS_InitThread(thread, task, thread, thread, tid, 0, 0, priority) // TODO: deide how to handle stacks
    _RunPt = thread;
  }
  else {
    _TCB* temp = _RunPt;  // store _RunPt in case thread switcher changes it while this is happening
    // for simplicity, add new thread immediately after RunPt
    _OS_InitThread(thread, task, temp->next, temp, tid, 0, 0, priority)
    temp->next->prev = thread;
    temp->next = thread;
  }
  // unlock thread linked list
  OS_bSignal(_modifyTCB);
  return 1;
}

// Initialize a thread
// takes as arguments the thread to initialize and a value for each field in the thread
// static void _OS_InitThread(_TCB* thread, void(*task)(void), _TCB* next, _TCB* prev, int id, char sleep, char block, unsigned long priority) {
//   _OS_SetInitialStack(thread, task);
//   thread->next = next;
//   thread->prev = prev;
//   thread->id = id;
//   thread->sleep = sleep;
//   thread->block = block;
//   thread->priority = priority;
//}

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, select switch and timer2 
// input:  none
// output: none
void OS_Init(void) {
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
  _modifyTCB = OS_InitSemaphore(OS_BINARY_SEMAPHORE);
  _RunPt = NULL;
  _OS_numThreads = 0;
  
  /* Add default thread in case all threads killed */
  OS_AddThread(&_OS_Default_Thread, 0, 0); // should be lowest priority
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
  static int i = 0;
  UART_OutString("threads:\r\n");
  _OS_PrintThreads(3);
//   UART_OutUDec(_RunPt->id); UART_OutString(SH_NL);
//   UART_OutUDec(_RunPt->next->id); UART_OutString(SH_NL);
//   UART_OutUDec(_RunPt->next->next->id); UART_OutString(SH_NL);
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
	_TCB *temp;
	OS_bWait(_modifyTCB);
	_RunPt->prev->next = _RunPt->next;
	_RunPt->next->prev = _RunPt->prev;
	temp = _RunPt;
	_RunPt = _RunPt->next;
	free(temp);
	OS_bSignal(_modifyTCB);
  NVIC_ST_CURRENT_R = 0x0; // any write clears
  NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; // trigger pendSV interrupt
}

static void _OS_Default_Thread(void) {
  while(1)
   ;// OS_Suspend();
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
}

// OS_Delay
// count up to specified value
// should be used after calls to OS_Kill, Suspend, etc
void OS_Delay(int count) {
  int i;
  for(i = 0; i < count; i++)
    ;
}