#include "shell.h"
#include "UART.h"
#include "OS.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>

#if DEBUG == 1
	#include "rit128x96x4.h"
#endif

typedef struct {
  char name[10], val[25];
	char set;
} _SH_Env_Var;

typedef struct {
	char command[25];
	char arguments[10][25];
} _SH_Command;

static _SH_Env_Var* _SH_setVar(const char* varName, const char* newVal);
static char* _SH_getVar(const char* varName);
static _SH_Env_Var* _SH_findVar(const char* varName);
static int _SH_Execute(_SH_Command command);
static _SH_Command _SH_Create_Command(char* input);
static _SH_Command _SH_Parse_Command(const char* input);

static _SH_Env_Var _SH_Env[SH_NUM_VARS];

/* Retrieves the value of an environment variable.
 * param: const char* varName, name of variable to retrieve
 * return: value of variable, null string if not set
 */
static char* _SH_getVar(const char* varName)
{
  _SH_Env_Var* temp = _SH_findVar(varName);
  if (temp != SH_NULL && temp->set == 1) {
    #if DEBUG == 1
      OLED_Out(TOP, "Got");
      OLED_Out(TOP, temp->name);
      OLED_Out(TOP, "value:");
      OLED_Out(TOP, temp->val);
    #endif
    return temp->val;
  }
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
  #if DEBUG == 1
    OLED_Out(BOTTOM, "Set");
    OLED_Out(BOTTOM, var->name);
    OLED_Out(BOTTOM, "value:");
    OLED_Out(BOTTOM, var->val);
    OLED_Out(BOTTOM, val);
  #endif
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
	
	UART_Init();
	_SH_setVar(SH_PROMPT_NAME, ">");
	while(1)
	{
		char input[SH_MAX_LENGTH] = {0};
		/* Show prompt */
		UART_OutString(SH_NL), UART_OutString(_SH_getVar(SH_PROMPT_NAME));
		/* Input command */
		UART_InString(input, SH_MAX_LENGTH);
		UART_OutString(SH_NL);
		/* Construct and execute command */
		_SH_Execute(_SH_Parse_Command(input));
    memset(input, 0, SH_MAX_LENGTH);
  }
}

/* Maybe hash the commands so O(1) lookup time */
static int _SH_Execute(_SH_Command command)
{
  int exitCode = 0;
  char buff[2] = {0};
  switch (command.command[0]) {
    case 's':
      exitCode = (_SH_setVar(command.arguments[0], command.arguments[1]) == SH_NULL);
      break;
    case 'e':
      UART_OutString(_SH_getVar(command.arguments[0]));
			exitCode = 0;
      break;
    case 't':
      UART_OutUDec(OS_MsTime());
			exitCode = 0;
      break;
    default:
      UART_OutString(SH_INVALID_CMD);
      UART_OutString(command.command);
      UART_OutString(SH_NL);
			exitCode = 1;
      break;
  }
  buff[0] = exitCode + 0x30;
//  _SH_setVar("?", buff);
	return exitCode;
}

static _SH_Command _SH_Create_Command(char* input)
{
	_SH_Command command;
  char * delims = " ";
	char* pt;
	
	//input[strlen(input)-1] = '\0';
	pt = strtok(input, delims);
	strcpy(command.command, pt);
	#if DEBUG == 1
    UART_OutString(command.command);
    UART_OutString(SH_NL);
		OLED_Out(TOP, command.command);
	#endif
	pt = strtok(NULL, delims);
	while(pt != NULL)
	{
		static int i = 0;
		strcpy(command.arguments[i++], pt);
		pt = strtok(NULL, delims);
		#if DEBUG == 1
      UART_OutString(command.arguments[i-1]);
      UART_OutString(SH_NL);
			OLED_Out(TOP, command.arguments[i-1]);
		#endif
	}
	return command;
}

static _SH_Command _SH_Parse_Command(const char* input) {
  _SH_Command command;
  int base, offset, argNum = 0;
  base = offset = 0;
  while(input[offset] != 0 && input[offset++] != ' ' ) {
    ;
  }
  base = offset;
  memcpy(command.command, input, offset);
  command.command[offset] = 0;
  while(input[offset] != 0) {
    while(input[offset] != 0 && input[offset] != ' ')  {
      offset++;
    }
    memcpy(command.arguments[argNum], &input[base], offset - base);
    command.arguments[argNum++][offset - base] = 0;
    UART_OutString(command.arguments[argNum - 1]);
    base = ++offset;
  }
  return command;  
}
