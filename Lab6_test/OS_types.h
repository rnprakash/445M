#ifndef __OS_TYPES_H__
#define __OS_TYPES_H__

/* Task node for linked list */
typedef struct _OS_Task {
	void (*task)(void);			/* Periodic task to perform */
  int task_id;		        /* Task id */
	unsigned long time,			/* _OS_Task_Time at which to perform */
							priority,		/* Task priority */
							period;			/* Frequency in units of 100ns */
	struct _OS_Task *next;	/* Pointer to next task to perform*/
} _OS_Task;

/* Thread struct */
typedef struct _TCB {
  volatile unsigned long * sp;    /* stack pointer */
	struct _TCB * next, *prev; 			/* Link pointers */
	int id;													/* Thread id */
	char sleep, block, run;					/* Flags for run, sleep, and block states */
	unsigned long priority;					/* Thread priority */
	unsigned long base_priority;		/* Thread base priority */
  unsigned long sleepTime;        /* if sleeping, the minimum number of ms until woken up */
	volatile unsigned long stack[_OS_STACK_SIZE];	/* Pointer to thread's stack */
} _TCB;

typedef struct OS_SemaphoreType {
  long value;
  _TCB *blockedThreads[_OS_MAX_THREADS + 1]; // queue of threads blocked on this semaphore?
  int GetIndex, PutIndex;
} OS_SemaphoreType;

typedef struct _OS_FifoType {
  unsigned long Fifo[_OS_FIFO_SIZE];
  unsigned long size;
  unsigned int PutIndex, GetIndex;
  OS_SemaphoreType notEmpty;
  OS_SemaphoreType mutex;
} _OS_FifoType;

typedef struct _OS_MailBoxType {
  unsigned long data;
  OS_SemaphoreType hasData;
  OS_SemaphoreType gotData;
} _OS_MailboxType;

typedef struct _OS_Event {
  unsigned long timestamp;
  char type;
} _OS_Event;

#endif
