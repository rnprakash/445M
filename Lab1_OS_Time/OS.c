#include "hw_types.h"
#include "sysctl.h"
#include "lm3s8962.h"
#include "stdlib.h"
#include "OS_Critical.h"
#include "OS.h"
#include "rit128x96x4.h"

typedef struct _OS_Task {
	void (*task)(void);
	unsigned long time,
							priority,
							period;
	struct _OS_Task *next;
} _OS_Task;

static int _GCF(_OS_Task* root, int index);
static void _OS_Inc_Time(void);

/* Task linked list */
static _OS_Task* _OS_Root;
static _OS_Task* _OS_Tail;
static int _OS_Task_Time = 0;

/* System time */
static unsigned int _OS_System_Time;

/* Initialize timer2 for system time */
void Timer2_Init()
{
	int nop = 5;
	SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2;	/* Activate timer2A */
	nop *= SYSCTL_RCGC1_TIMER2;							/* Wait for clock to activate */
	TIMER2_CTL_R &= ~0x00000001;						/* Disable timer2A during setup */
	TIMER2_CFG_R = 0x00000004;							/* Configure for 16-bit timer mode */
	TIMER2_TAMR_R = 0x00000002;							/* Configure for periodic mode */
	TIMER2_TAPR_R = 4;											/* 100ns timer2A */
	TIMER2_ICR_R = 0x00000001;							/* Clear timer2A timeout flag */
	TIMER2_IMR_R |= 0x00000001;							/* Arm timeout interrupt */
	NVIC_EN0_R |= NVIC_EN0_INT23;						/* Enable interrupt 23 in NVIC */
	
	/* Add system time task */
	OS_Add_Periodic_Thread(&_OS_Inc_Time, 1000, 4);
}

/*
 * Adds 
 */
int OS_Add_Periodic_Thread(void (*task)(void), unsigned long period, unsigned long priority)
{
	/* Num tasks */
	static int _OS_Num_Tasks;
	/* Allocate variables */
	OS_CPU_SR cpu_sr;
	_OS_Task *new_task, *temp_node;
	int /*new_period,*/ new_priority, offset;

	/* Bounds checking */
	if(_OS_Num_Tasks++ >= OS_MAX_TASKS)
		return 1;

	/* Set task parameters */
	new_task = (_OS_Task*)malloc(sizeof(_OS_Task) + 1);
	new_task->task = task;
	new_task->period = period * 10;
	new_task->priority = priority;
	new_task->time = 0;
	
	/* Create linked list of threads in order.
   * Compare current index's time of execution
	 * with current time. If matched (or surpassed)
	 * execute task. Else, increment time. 
	 */
	if(_OS_Root == 0)
	{
		_OS_Root = new_task;
		_OS_Tail = new_task;
	}
	else
	{
		_OS_Tail->next = new_task;
		_OS_Tail = _OS_Tail->next;
	}
	
	/* Calculate outside critical section */
	//new_period = _GCF(_OS_Root, _OS_Num_Tasks) /*_OS_Num_Tasks*/;
	new_priority = ((NVIC_PRI5_R&0x00FFFFFF)
							| (1 << (28 + priority)));
	
	/* Start critical section */
	OS_ENTER_CRITICAL();

	/* Update timing intervals */
	TIMER2_TAILR_R = _GCF(_OS_Root, _OS_Num_Tasks);
	/* Set priority based on next interrupt */
	NVIC_PRI5_R = new_priority;
	/* Enable timer (in case this is the first task) */
	TIMER2_CTL_R |= 0x00000001;

	/* Update each node's time */
	temp_node = _OS_Root;
	offset = _OS_Root->time;
	while(temp_node)
	{
		temp_node->time = offset++;
		temp_node = temp_node->next;
	}
	
	/* Update task time */
	_OS_Task_Time = 0;
	
	/* End critical section */
	OS_EXIT_CRITICAL();
	return 0;
}

static /* inline */ int _GCF(_OS_Task* root, int index)
{
	return 1;
}

void Timer2_Handler(void)
{
	_OS_Task *cur_task = _OS_Root, *temp;
	OS_CPU_SR cpu_sr;
	
	/* Update task time */
	_OS_Task_Time++;
	
	if(_OS_Task_Time < _OS_Root->time)
		return;
	
	/* Execute task */
	cur_task->task();
	
	/* Begin critical section */
	OS_ENTER_CRITICAL();
	
	/* Update list */
	_OS_Root = _OS_Root->next;
	cur_task->time += cur_task->period;
	while(temp->next && temp->next->time < cur_task->time)
	{
		temp = temp->next;
	}
	cur_task->next = temp->next;
	temp->next = cur_task;
	
	/* Update priority */
	NVIC_PRI5_R = _OS_Root->priority;
	
	/* End critical section */
	OS_EXIT_CRITICAL();
}

static void _OS_Inc_Time()
{
	_OS_System_Time++;
}

unsigned int OS_Time()
{
	return _OS_System_Time;
}

static int buff[100];
static int index = 0;

void func1(void)
{
	buff[index++] = 1;
	index %= 100;
	//OLED_Out(TOP, "1");
}

void func2(void)
{
	buff[index++] = 2;
	index %= 100;
	//OLED_Out(BOTTOM, "2");
}

int main(void)
{
	Output_Init();
	OLED_Set_Color(15);
	OLED_Out(TOP, "1");
	//Timer2_Init();
	//OS_Add_Periodic_Thread(&func1, 1000000, 5);
	//OS_Add_Periodic_Thread(&func2, 2000000, 5);
	while(1);
}
