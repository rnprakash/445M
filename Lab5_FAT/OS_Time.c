#include "OS.h"
#include "OS_Critical.h"
#include "hw_types.h"
#include "lm3s8962.h"
#include <stdlib.h>

static void _OS_Inc_Time(void);
static void _OS_Update_Root(_OS_Task * temp, _OS_Task * cur_task);

extern _TCB _threads[_OS_MAX_THREADS]; // static storage for threads
extern _OS_Task _tasks[OS_MAX_TASKS]; // static storage for periodic threads

static long MaxJitter[OS_MAX_TASKS] = {0,};    // largest time difference between interrupt trigger and running thread
static long MinJitter[OS_MAX_TASKS] = {10000000,};    // smallest time difference between interrupt trigger and running thread
static unsigned long const JitterSize = JITTERSIZE;
static unsigned long JitterHistogram[OS_MAX_TASKS][JITTERSIZE];

/***   Variable declarations   ***/
/* Linked list of tasks */
_OS_Task* _OS_Root = NULL;
/* Interrupt counter, tasks executed based on this */
unsigned int _OS_Task_Time = 0;
/* OS system time */
extern unsigned int _OS_System_Time;
/*** End variable declarations ***/

/* Initialize timer2 for system time */
#pragma O0
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
#pragma O0
void Timer2B_Init(int priority) {
  int nop = 5;
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2;	/* Activate timer2 */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	TIMER2_CTL_R &= ~0x00000100;						/* Disable timer2B during setup */
	TIMER2_CFG_R = 0x00000004;							/* Configure for 16-bit timer mode */
	TIMER2_TBMR_R = 0x00000002;							/* Configure for periodic mode */
	TIMER2_TBPR_R = 49;											/* 1 us timer2B */
	TIMER2_ICR_R |= 0x00000100;							/* Clear timer2B timeout flag */
	TIMER2_TBILR_R |= 0x0000FFFF;									/* maximum reload time */
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
					 -1 if maximum threads already queued.
 */
int OS_Add_Periodic_Thread(void (*task)(void), unsigned long period, unsigned long priority)
{
	/* Declare function as critical */
	OS_CRITICAL_FUNCTION;
	
	/* Number of scheduled tasks */
	static int _OS_Num_Tasks = 0;
	
	/* Allocate variables */
	_OS_Task *new_task = &_tasks[0];
	int new_priority, i;

	/* Bounds checking */
	if(_OS_Num_Tasks >= OS_MAX_TASKS)
		return -1;

	/* Allocate space for new task */
  // find free space in static storage array
  for(i = 0; i < OS_MAX_TASKS; i++) {
    if(_tasks[i].task_id == _OS_FREE_THREAD) {
      new_task = & _tasks[i];
      break;
    }
  }
  if(i == OS_MAX_TASKS) {
    // maximum storage already allocated
    return -1;
  }
// 	new_task = (_OS_Task*)malloc(sizeof(_OS_Task) + 1);
	
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
  
  // Record Jitter Measurements
  // period is in ms, jitter is recorded in us
//   MeasureJitter(cur_task->task_id, (cur_task->period) * 1000);
	
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
__attribute__((long_call, section(".data"))) unsigned int OS_MsTime(void)
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

// ******** OS_Time ************
// reads a timer value 
// Inputs:  none
// Outputs: time in 20ns units, 0 to max
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void) {
  return TIMER2_TBR_R & 0xFFFF;
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 20ns units 
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
long OS_TimeDifference(unsigned long start, unsigned long stop) {
  if(start > stop) {
    return start - stop;
  }
  else {
    return start + (0xFFFF - stop); // 0xFFFF is the maximum timer value
  }
}

// measure & record the jitter for a given periodic task
// period should be in units of us
void OS_MeasureJitter(int taskID, unsigned long period) {
  static unsigned long LastTime[OS_MAX_TASKS];
  static int firstTime = -1;  // each bit is a boolean for a task
//  static char firstTime[OS_MAX_TASKS] = {1, 1, 1, 1};
  int index;
  long jitter;                    // time between measured and expected
  unsigned long thisTime = OS_Time();
  if((firstTime >> taskID) & 0x01) {
    firstTime = firstTime & ~(0x01 << taskID);
//    firstTime[taskID] = 0;
  }
  else {
    jitter = OS_TimeDifference(LastTime[taskID],thisTime) - period; // in us
    if(jitter > MaxJitter[taskID]){
      MaxJitter[taskID] = jitter;
    }
    if(jitter < MinJitter[taskID]){
      MinJitter[taskID] = jitter;
    }        // jitter should be 0
    index = jitter+JITTERSIZE/2;   // us units
    if(index<0)
      index = 0;
    if(index>=JitterSize)
      index = JITTERSIZE-1;
    JitterHistogram[taskID][index]++;
  }
  LastTime[taskID] = thisTime;
}

long OS_getMaxJitter(int taskID) {
  return MaxJitter[taskID];
}

long OS_getMinJitter(int taskID) {
  return MinJitter[taskID];
}

static unsigned long _OS_int_time = 0;
static unsigned long _OS_cur_time = 0;

void OS_start_interrupt(void)
{
	_OS_cur_time = OS_Time();
}

void OS_end_interrupt(void)
{
	unsigned long temp = OS_Time();
	temp = OS_TimeDifference(temp, _OS_cur_time);
	if(temp > _OS_int_time)
		_OS_int_time = temp;
}

unsigned long OS_max_int_time(void)
{
	return _OS_int_time;
}
