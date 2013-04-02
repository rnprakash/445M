#include "shell.h"
#include "UART.h"
#include "OS.h"
#include "debug.h"
#include "OS_Critical.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if DEBUG == 1
	#include "rit128x96x4.h"
#endif

// extern _OS_Task _tasks[OS_MAX_TASKS]; // static storage for periodic threads
extern _OS_Event _eventLog[_OS_MAX_EVENTS]; // log timestamp data for events
extern int _eventIndex;  // index for event log

typedef struct {
  char name[10], val[25];
	char set;
} _SH_Env_Var;

typedef struct {
	char command[25];
	char arguments[5][10];
} _SH_Command;

static _SH_Env_Var* _SH_setVar(const char* varName, const char* newVal);
static char* _SH_getVar(const char* varName);
static _SH_Env_Var* _SH_findVar(const char* varName);
static int _SH_Execute(char *command);
//static _SH_Command _SH_Create_Command(char* input);
//static _SH_Command _SH_Parse_Command(const char* input);

static _SH_Env_Var _SH_Env[SH_NUM_VARS];

/* Retrieves the value of an environment variable.
 * param: const char* varName, name of variable to retrieve
 * return: value of variable, null string if not set
 */
static char* _SH_getVar(const char* varName)
{
  _SH_Env_Var* temp = _SH_findVar(varName);
  if (temp != SH_NULL && temp->set == 1)
    return temp->val;
  return "Value not set";
}

/* Sets an environment variable
 * param: const char* varName, variable name to set
 * param: const char* newVal, value of variable
 * return: pointer to environment variable.
						if no space to store variable, return SH_ERROR
 */
static _SH_Env_Var* _SH_setVar(const char* name, const char* val)
{
	_SH_Env_Var* var = _SH_findVar(name);
	if(var == SH_NULL)
		return SH_NULL;
	strcpy(var->name, name);
	strcpy(var->val, val);
	var->set = 1;
	return var;
}

/* Find variable in environment array. If not found, attempt to find space for it.
 * param: const char* varName, variable to find.
 * return: pointer to environment variable
 */
static _SH_Env_Var* _SH_findVar(const char* varName)
{
  int i;
  for(i = 0; i < SH_NUM_VARS; i++)
    if((_SH_Env[i].set == 1) && (strcmp(_SH_Env[i].name, varName) == 0))
      return &_SH_Env[i];
	for(i = 0; i < SH_NUM_VARS; i++)
      if(_SH_Env[i].set == 0)
        return &_SH_Env[i];
  return SH_NULL;
}
 
void SH_Init(void)
{
	int i;
	for(i = 0; i < SH_NUM_VARS; i++)
		_SH_Env[i].set = 0;
	
	UART_Init();  // called in main program
	_SH_setVar(SH_PROMPT_NAME, ">");
}

void SH_Shell(void) {
  OS_CRITICAL_FUNCTION;
  while(1)
	{
		char input[SH_MAX_LENGTH] = {0};
		/* Show prompt */
		UART_OutString(SH_NL), UART_OutString(_SH_getVar(SH_PROMPT_NAME));
		/* Input command */
		UART_InString(input, SH_MAX_LENGTH);
    OS_ENTER_CRITICAL();
		OS_start_interrupt();
		UART_OutString(SH_NL);
		/* Construct and execute command */
	//	_SH_Parse_Command(input);
    _SH_Execute(input);
		OS_end_interrupt();
    OS_EXIT_CRITICAL();
    memset(input, 0, SH_MAX_LENGTH);
    OS_Suspend();
  }
}

/* Maybe hash the commands so O(1) lookup time */
static int _SH_Execute(char *command)
{
  int exitCode = 0;
  //char buff[2] = {0};
  switch (command[0]) {
    case 's':
//      exitCode = (_SH_setVar(command.arguments[0], command.arguments[1]) == SH_NULL);
      break;
    case 'e':
//      UART_OutString(_SH_getVar(command.arguments[0]));
			exitCode = 0;
      break;
    case 't':
      // "time" : print OS time in milliseconds
      UART_OutUDec(OS_MsTime()); UART_OutString(" ms");
			exitCode = 0;
      break;
    case 'c':
      // "clear" : clears OS time
      OS_ClearMsTime(); UART_OutString("ms time cleared");
      exitCode = 0;
      break;
    case 'l':
      // "log" : print OS event log
      exitCode = SH_Log();
      break;
    case 'q':
      // clear OS event log
      exitCode = SH_ClearLog();
      break;
    default:
      UART_OutString(SH_INVALID_CMD);
      UART_OutString(command);
			exitCode = 1;
      break;
  }
//	buff[0] = exitCode + 0x30;
//	_SH_setVar("?", buff);
	return exitCode;
}

/*static _SH_Command _SH_Parse_Command(const char* input) {
  _SH_Command command;
  int base, offset, argNum = 0;
	
  base = offset = 0;
  while(input[offset] != 0 && input[offset++] != ' ' );
  base = offset;
  memcpy(command.command, input, offset);
  command.command[offset] = 0;
  
	while(input[offset] != 0) {
    while(input[offset] != 0 && input[offset] != ' ')  {
      offset++;
    }
    memcpy(command.arguments[argNum], &input[base], offset - base);
    command.arguments[argNum++][offset - base] = 0;
//     UART_OutString(command.arguments[argNum - 1]);
    base = ++offset;
  }
  return command;  
}*/

// prints jitter information
void SH_Jitter(void) {
  //int i;
  //char str[32];
  UART_OutString("Jitter Measurements:\r\n");
//   for(i = 0; i < OS_MAX_TASKS; i++) {
//     // print out jitter information for each periodic task
//     if(_tasks[i].task_id != _OS_FREE_THREAD) {
//       int id = _tasks[i].task_id;
//       sprintf(str, "Task %d:\r\n", id);      
//       UART_OutString(str);
//       sprintf(str, "MaxJitter = %d, MinJitter = %d\r\n", OS_getMaxJitter(id), OS_getMinJitter(id));
//       UART_OutString(str);
//     }
//   }
}

// print the event log information
int SH_Log(void) {
  int i;
  char str[32];
  UART_OutString("Event Log:\r\n");
  for(i = 0; i < _eventIndex; i++) {
    unsigned long timeStamp = _eventLog[i].timestamp;
    switch(_eventLog[i].type) {
      case EVENT_FIFO_PUT:
        sprintf(str, "Fifo Put at %d us\r\n", timeStamp);
        break;
      case EVENT_FIFO_GET:
        sprintf(str, "Fifo Get at %d us\r\n", timeStamp);
        break;
      case EVENT_FIFO_WAIT:
        sprintf(str, "Wait on Fifo Semaphore at %d us\r\n", timeStamp);
        break;
      case EVENT_FIFO_WAKE:
        sprintf(str, "Acquired Fifo Semaphore at %d us\r\n", timeStamp);
        break;
      case EVENT_CONSUMER_RUN:
        sprintf(str, "Consumer thread switched to at %d us\r\n", timeStamp);
        break;
      case EVENT_CONSUMER_GOT:
        sprintf(str, "Consumer got data from fifo at %d us\r\n", timeStamp);
        break;
      case EVENT_OLED_START:
        sprintf(str, "Began writing to OLED at %d us\r\n", timeStamp);
        break;
      case EVENT_OLED_FINISH:
        sprintf(str, "Finished writing to OLED at %d us\r\n", timeStamp);
        break;
      case EVENT_THREAD + 0:
        sprintf(str, "Switched to thread %d at %d us\r\n", 0, timeStamp);
        break;
      case EVENT_THREAD + 1:
        sprintf(str, "Switched to thread %d at %d us\r\n", 1, timeStamp);
        break;
      case EVENT_THREAD + 2:
        sprintf(str, "Switched to thread %d at %d us\r\n", 2, timeStamp);
        break;
      case EVENT_THREAD + 3:
        sprintf(str, "Switched to thread %d at %d us\r\n", 3, timeStamp);
        break;
      default:
        sprintf(str, "Unrecognized event at %d us\r\n", timeStamp);
        break;
    }
    UART_OutString(str);
  }
  return 0;
}

// clear the OS event log
int SH_ClearLog(void) {
  memset(_eventLog, 0, sizeof(_OS_Event) * _eventIndex);
  _eventIndex = 0;
  return 0;
}
