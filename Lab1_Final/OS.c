#include "hw_types.h"
#include "lm3s8962.h"
#include "stdlib.h"
#include "OS_Critical.h"
#include "OS.h"
#include "Debug.h"

#if DEBUG == 1
	#include "stdio.h"
	#include "rit128x96x4.h"
#endif

/* Task node for linked list */
typedef struct _OS_Task {
	void (*task)(void);			/* Periodic task to perform */
	unsigned long time,			/* _OS_Task_Time at which to perform */
							priority,		/* Task priority */
							period,			/* Frequency in units of 100ns */
							task_id;		/* Task id */
	struct _OS_Task *next;	/* Pointer to next task to perform*/
} _OS_Task;

/* Increment the OS system time with each call
 * param: none
 * return none
 */
static void _OS_Inc_Time(void);

static void _OS_Update_Root(_OS_Task * temp, _OS_Task * cur_task);

/* Dummy function for profiling the Timer2 ISR */
static void dummy(void);

/* Linked list of tasks */
static _OS_Task* _OS_Root = NULL;

/* Interrupt counter, used to determine when scheduled
    tasks are to be executed                            */
static int _OS_Task_Time = 0;

/* OS system time */
static unsigned int _OS_System_Time;

/* Initialize timer2 for system time */
void Timer2A_Init(void)
{
	int nop = 5;
  #if DEBUG == 1
    volatile unsigned long delay;
  #endif
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
  
  #if DEBUG == 1
    /* setup PB0 to profile the timer2 ISR */
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
    delay = SYSCTL_RCGC2_R;
    GPIO_PORTB_DIR_R |= 0x01;
    GPIO_PORTB_DEN_R |= 0x01;
    GPIO_PORTB_DATA_R &= ~0x01;
  #endif
	
	/* Add system time task */
	OS_Add_Periodic_Thread(&_OS_Inc_Time, 1, 4);
//  OS_Add_Periodic_Thread(&dummy, 100, 1);
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
  
  #if DEBUG == 1
    GPIO_PORTB_DATA_R |= 0x01;
  #endif
	
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
  
  #if DEBUG == 1
    GPIO_PORTB_DATA_R &= ~0x01;
  #endif
	
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
	#if DEBUG == 1
	char time[20];
	#endif
	
	_OS_System_Time++;
	
	#if DEBUG == 1
// 	if(_OS_System_Time % 1000 == 0)
// 	{
// 		sprintf(time, "Time: %d", _OS_System_Time);
// 		OLED_Out(BOTTOM, time);
// 	}
	#endif
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

/* Dummy function for profiling the Timer2 ISR */
static void dummy(void) {
}
