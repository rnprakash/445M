#include "shell.h"
#include "ADC.h"
#include "UART.h"
#include "OS.h"
#include "debug.h"
#include "OS_Critical.h"
#include "eFile.h"
#include "eDisk.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define memcpy memmove

extern _OS_Event _eventLog[_OS_MAX_EVENTS]; // log timestamp data for events
extern int _eventIndex;  // index for event log

static char _SH_Buffer[512];
static _SH_Command _SH_cmd;
static _SH_Env_Var _SH_Env[SH_NUM_VARS];

static _SH_CommandPtr _SH_CommandList[] = {
	{"time", &_SH_Time},
	{"clear_time", &_SH_ClearTime},
	{"log", &_SH_Log},
	{"clear_log", &_SH_ClearLog},
	{"sd_format", &_SH_Format},
	{"write", &_SH_Write},
	{"read", &_SH_Read},
	{"cat", &_SH_Read},
	{"rm", &_SH_Rm},
	{"touch", &_SH_Create},
	{"mkdir", &_SH_MakeDirectory},
	{"ls", &_SH_DirectoryList},
	{"clear", &_SH_Clear},
	{"set", &_SH_Set},
	{"unset", &_SH_Unset},
	{"echo", &_SH_Echo},
	{"./", &_SH_BinFile},
	{"fload", &_SH_LoadBin},
	{"hexdump", &_SH_HexDump},
	{"reset", &_SH_Reset},
	{"sector", &_SH_SectorDump},
	{"diskinfo", &_SH_DiskInfo},
	{"cd", &_SH_ChangeDirectory},
	{"eject", &eFile_Close},
	{"init", &eFile_Init},
	{"",0}
};

/* Retrieves the value of an environment variable.
 * param: const char* varName, name of variable to retrieve
 * return: value of variable, null string if not set
 */
char* _SH_getVar(const char* varName)
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
	
	UART_Init();
	printf("\nShell startup\n");
	_SH_setVar(SH_PROMPT_NAME, ">");
	printf("\n");
//   OS_AddThread(&SH_Shell,128,3);
}

static char input[SH_MAX_LENGTH] = {0};
void SH_Shell(void) {
  SH_Init();
  while(1)
	{
		/* Show prompt */
		printf("%s", _SH_getVar(SH_PROMPT_NAME));
		/* Input command */
		_SH_InCommand(input, SH_MAX_LENGTH);
		printf("\n");
		/* Construct and execute command */
		if(!input[0])
			continue;
		_SH_Parse_Command(input);
		_SH_BeginRedirect(); // determine file redirection
    _SH_Execute();
		_SH_EndRedirect(); // end redirect to file (if necessary)
    memset(input, 0, SH_MAX_LENGTH);
    OS_Suspend();
  }
}


static char fBuff[13];
char* SH_AutoCompleteCommand(const char* start, int len)
{
	int i;
	memset(fBuff, 0, 13);
	for(i = 0; _SH_CommandList[i].command[0] != 0; i++)
	{
		memcpy(fBuff, _SH_CommandList[i].command, len);
		if(strcmp(fBuff, start) == 0)
		{
			strcpy(fBuff, _SH_CommandList[i].command);
			return fBuff + len;
		}
	}
	fBuff[0] = 0;
	return fBuff;
}

char list[MAX_FILES][13];
char* SH_AutoCompleteFile(const char* start, int len)
{
	int i;
	memset(fBuff, 0, 13);
	eFile_List(list);
	for(i = 0; list[i][0] != 0; i++)
	{
		memcpy(fBuff, list[i], len);
		if(strcmp(fBuff, start) == 0)
		{
			strcpy(fBuff, list[i]);
			return fBuff + len;
		}
	}
	fBuff[0] = 0;
	return fBuff;
}

static int _SH_Redirected = 0;
static void _SH_BeginRedirect(void)
{
	int i;
	for(i = 0; i < SH_MAX_ARGS - 1; i++)
	{
		if(_SH_cmd.args[i][0] == '>')
		{
			if((_SH_cmd.args[i][1] == '>' && _SH_cmd.args[i][2]) || !_SH_cmd.args[i+1][0])
			{
				printf("Invalid file redirection\n");
				return;
			}
			if(_SH_cmd.args[i][1] == '\0')
				eFile_Delete(_SH_cmd.args[i+1]);
			eFile_RedirectToFile(_SH_cmd.args[i+1]);
			_SH_Redirected = 1;
			_SH_cmd.args[i][0] = 0;
			return;
		}
	}
}

static void _SH_EndRedirect(void)
{
	if(_SH_Redirected)
		eFile_EndRedirectToFile();
	_SH_Redirected = 0;
}

/* Maybe hash the commands so O(1) lookup time */
static int _SH_Execute(void)
{
  int i;
	for(i = 0; _SH_CommandList[i].command[0] != 0; i++)
	{
		if(strcmp(_SH_CommandList[i].command, _SH_cmd.command) == 0)
			return _SH_CommandList[i].func();
	}
	fprintf(stderr, "%s: command not found\n", _SH_cmd.command);
	return 1;
}

static void _SH_Parse_Command(const char* input) {
  int base, offset, argNum = 0;
	
	memset(&_SH_cmd, 0, sizeof(_SH_Command));
  base = offset = 0;
  while(input[offset] != 0 && input[offset] != ' ' )
		offset++;
	
  base = offset + 1;
  memcpy(_SH_cmd.command, input, offset);
  _SH_cmd.command[offset++] = 0;
  
	while(input[offset] != 0 && argNum < SH_MAX_ARGS) {
    while(input[offset] != 0 && input[offset] != ' ')  {
      offset++;
    }
    memcpy(_SH_cmd.args[argNum], &input[base], offset - base);
    _SH_cmd.args[argNum++][offset - base] = 0;
    base = ++offset;
  }
}

static char _SH_History[SH_HISTORY][SH_MAX_LENGTH] = {{0}};
static int index = 0;
static void _SH_InCommand(char *bufPt, unsigned short max) {
	int length = 0;
	int space = 0;
	char character;
	char* startPt = bufPt;
	char* word = bufPt;
  character = UART_InChar();
  while(character != CR && character != LF && character != CTRL_C){
		if(character == ' ')
		{
			space = length;
			word = bufPt+1;
		}
    if(character == BS || character == DEL){
      if(length){
        bufPt--;
        length--;
				printf("\b \b");
      }
    }
		else if(character == CTRL_L)
		{
			int i;
			printf("\f%s", _SH_getVar(SH_PROMPT_NAME));
			for(i = 0; i < length; i++)
				UART_OutChar(startPt[i]);
		}
		else if(character == CTRL_U)
		{
			int i;
			for(i = 0; i < length + 1; i++)
			{
				printf ("\b \b");
			}
			printf("%s", _SH_getVar(SH_PROMPT_NAME));
			memset(startPt, 0, max);
			length = 0;
			bufPt = startPt;
			word = startPt;
			space = 0;
		}
		else if(character == '\t')
		{
			char fBuff[8];
			if(space) // tab complete file name
			{
				char *c = fBuff;
				memcpy(fBuff, word, length - space);
				fBuff[length - space] = 0;
				strcpy(fBuff, SH_AutoCompleteFile(fBuff, length - space - 1));
				while(*c)
				{
					*bufPt = *c;
					bufPt++;
					length++;
					UART_OutChar(*c++);
				}
			}
			else // tab complete command name
			{
				char *c = fBuff;
				memcpy(fBuff, startPt, length);
				fBuff[length] = 0;
				strcpy(fBuff, SH_AutoCompleteCommand(fBuff, length));
				while(*c)
				{
					*bufPt = *c;
					bufPt++;
					length++;
					UART_OutChar(*c++);
				}
			}
		}
		else if(character == 0x42 && length > 1
						&& startPt[length-1] == 0x5B
						&& startPt[length-2] == 0x1B) // down arrow
		{
			int i;
			printf("%c%c%c%c",  0x41, 0x1B, 0x5B, 0x42);
			if(!_SH_History[(index+1)&(SH_HISTORY-1)][0])
			{
				length -= 2;
				bufPt -= 2;
				character = UART_InChar();
				continue;
			}
			for(i = 0; i < length - 2; i++)
			{
				printf ("\b \b");
			}
			printf("\r");
			index = (index + 1) % SH_HISTORY;
			length = strlen(_SH_History[index]);
			strcpy(startPt, _SH_History[index]);
			bufPt = startPt;
			word = startPt;
			space = 0;
			UART_OutString(_SH_getVar(SH_PROMPT_NAME));
			for(; bufPt < startPt + length; bufPt++)
			{
				UART_OutChar(*bufPt);
				if(*bufPt == ' ')
				{
					space = 1;
					word = bufPt+1;
				}
			}
		}
		else if(character == 0x41 && length > 1
						&& startPt[length-1] == 0x5B
						&& startPt[length-2] == 0x1B) // up arrow
		{
			int i;
			printf("%c%c%c%c",  0x41, 0x1B, 0x5B, 0x42);
			if(!_SH_History[(index-1)&(SH_HISTORY-1)][0])
			{
				length -= 2;
				bufPt -= 2;
				character = UART_InChar();
				continue;
			}
			for(i = 0; i < length - 2; i++)
			{
				printf ("\b \b");
			}
			printf("\r");
			index = (index - 1) % SH_HISTORY;
			length = strlen(_SH_History[index]);
			strcpy(startPt, _SH_History[index]);
			bufPt = startPt;
			word = startPt;
			space = 0;
			UART_OutString(_SH_getVar(SH_PROMPT_NAME));
			for(; bufPt < startPt + length; bufPt++)
			{
				UART_OutChar(*bufPt);
				if(*bufPt == ' ')
				{
					space = 1;
					word = bufPt+1;
				}
			}
		}
    else if(length < max){
      *bufPt = character;
      bufPt++;
      length++;
      UART_OutChar(character);
    }
    character = UART_InChar();
  }
	if(character == CTRL_C)
		*startPt = 0;
  *bufPt = 0;
	if(*startPt)
	{
		strcpy(_SH_History[index], startPt);
		index = (index + 1)&(SH_HISTORY-1);
	}
}

// prints jitter information
void SH_Jitter(void) {
  UART_OutString("Jitter Measurements:\r\n");
}

// print the event log information
static int _SH_Log(void) {
  int i;
  char str[64];
  UART_OutString("Event Log:\r\n");
  for(i = 0; i < _eventIndex; i++) {
    unsigned long timeStamp = _eventLog[i].timestamp;
    sprintf(str, "switched to thread %d at %d us \r\n", _eventLog[i].type, timeStamp);
    UART_OutString(str);
  }
    /*switch(_eventLog[i].type) {
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
  }*/
  return 0;
}

// clear the OS event log
static int _SH_ClearLog(void) {
  memset(_eventLog, 0, sizeof(_OS_Event) * _eventIndex);
  _eventIndex = 0;
  return 0;
}

static int(*p)(const char*format,...) = &printf;
__attribute__((long_call, section(".data"))) static int _SH_Time(void)
{
	p("%d\n", OS_MsTime());
	return 0;
}

static int _SH_ClearTime(void)
{
	OS_ClearMsTime();
	return 0;
}

static int _SH_Format(void)
{
	printf("Formatting disk...\n");
	if(eFile_Format()) fprintf(stderr, "File format failed!\n");
	return 0;
}

static int _SH_Write(void)
{
	int i;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <file_name>\n", _SH_cmd.command);
		return 1;
	}
	memset(_SH_Buffer, 0, 512);
	if(eFile_WOpen(_SH_cmd.args[0]))
	{
		fprintf(stderr, "Unable to open file for writing!\n");
		return 1;
	}
	UART_InString(_SH_Buffer, 512);
	printf("\nWriting to file %s...\n", _SH_cmd.args[0]);
  for(i = 0; i < 512 && _SH_Buffer[i]; i++)
	{
		if(eFile_Write(_SH_Buffer[i]))
		{
			fprintf(stderr, "Error writing to file!\n");
			eFile_WClose();
			return 1;
		}
	}
	eFile_Write('\n');
	eFile_WClose();
	return 0;
}

static int _SH_Read(void)
{
	char c;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <file_name>\n", _SH_cmd.command);
		return 1;
	}
	if(eFile_ROpen(_SH_cmd.args[0]))
	{
		fprintf(stderr, "Error opening file for reading\n");
		eFile_RClose();
		return 1;
	}
	while(!eFile_ReadNext(&c))
		printf("%c", c);
	eFile_RClose();
	printf("\n");
	return 0;
}

static int _SH_Rm(void)
{
	int i;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <file_name> ...\n", _SH_cmd.command);
		return 1;
	}
	for(i = 0; _SH_cmd.args[i][0] && i < SH_MAX_ARGS; i++)
		if(eFile_Delete(_SH_cmd.args[i]))
			fprintf(stderr, "%s: cannot remove file: %s\n", _SH_cmd.command, _SH_cmd.args[i]);
	return 0;
}

static int _SH_Create(void)
{
	int i;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <file_name> ...\n", _SH_cmd.command);
		return 1;
	}
	for(i = 0; _SH_cmd.args[i][0] && i < SH_MAX_ARGS; i++)
		if(eFile_Create(_SH_cmd.args[i], 0x20))
			fprintf(stderr, "%s: error creating file: %s\n", _SH_cmd.command, _SH_cmd.args[i]);
	return 0;
}

static int _SH_MakeDirectory(void)
{
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <name>\n", _SH_cmd.command);
		return 1;
	}
	return eFile_Create(_SH_cmd.args[0], 0x10);
}

static int _SH_DirectoryList(void)
{
	eFile_Directory(&printf);
	return 0;
}

static int _SH_Clear(void)
{
	fprintf(stderr, "\f");
	return 0;
}

static int _SH_Set(void)
{
	if(!_SH_cmd.args[0][0] || !_SH_cmd.args[1][0])
	{
		fprintf(stderr, "Usage: %s <variable> <value>\n", _SH_cmd.command);
	}
	_SH_setVar(_SH_cmd.args[0], _SH_cmd.args[1]);
	return 0;
}

static int _SH_Unset(void)
{
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <variable>\n", _SH_cmd.command);
	}
	_SH_setVar(_SH_cmd.args[0], "");
	return 0;
}

static int _SH_Echo(void)
{
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <$variable/string>\n", _SH_cmd.command);
	}
	if(_SH_cmd.args[0][0] == '$')
		printf("%s\n", _SH_getVar(&_SH_cmd.args[0][1]));
	else
		printf("%s\n", _SH_cmd.args[0]);
	return 0;
}

union {
	char c[1000];
	int i;
	int (*f)(void);
} funcBuff ;

static int _SH_BinFile(void)
{
	char c;
	int i = 0;
	int (*f)(void);
	void* v;
	
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <file_name>\n", _SH_cmd.command);
		return 1;
	}
	if(eFile_ROpen(_SH_cmd.args[0]))
	{
		fprintf(stderr, "Unable to open file for reading!\n");
		eFile_RClose();
		return 1;
	}
	i = 0;
	while(!eFile_ReadNext(&c))
		funcBuff.c[i++] = c;
	eFile_RClose();
	v = (void*)((int)&(funcBuff.c[0]) | 0x1);
	f = (int (*)(void))v;
	f();
	return 0;
}

static int _SH_LoadBin(void)
{
	int i;
	long* l;
	char* c = funcBuff.c;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <filename>\n", _SH_cmd.command);
		return 1;
	}
	funcBuff.f = &_SH_Time;
	l = (long*)(((c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0]) - 1);
	memcpy((void*)((int)&funcBuff.c[0]), l, 200);
	if(eFile_WOpen(_SH_cmd.args[0]))

	{
		fprintf(stderr, "Unable to open file for writing\n");
		return 1;
	}
	for(i = 0; i < 200; i++)
	{
		if(eFile_Write(funcBuff.c[i]))
		{
			fprintf(stderr, "Error writing to file\n");
			return 1;
		}
	}
	eFile_WClose();
	return 0;
}

static int _SH_HexDump(void)
{
	char c[4];
	int i, ret = 0;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <filename>\n", _SH_cmd.command);
		return 1;
	}
	if(eFile_ROpen(_SH_cmd.args[0]))
	{
		fprintf(stderr, "Error opening file for reading\n");
		eFile_RClose();
		return 1;
	}
	while(1)
	{
		for(i = 0; i < 4; i++)
		{
			unsigned long l;
			eFile_ReadNext(&c[0]);
			eFile_ReadNext(&c[1]);
			eFile_ReadNext(&c[2]);
			ret = eFile_ReadNext(&c[3]);
			l = ((c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0]);
			UART_OutUHex(l);
			printf("\n");
			if(ret || l == 0)
			{
				eFile_RClose();
				printf("\n");
				return 0;
			}
		}
	}
}


extern void Reset_Handler(void);
static int _SH_Reset(void)
{
	Reset_Handler();
	return 0;
}

static int _SH_SectorDump(void)
{
	int i;
	unsigned char* c = (unsigned char*)funcBuff.c;
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <sector number>\n", _SH_cmd.command);
		return 1;
	}
	eDisk_ReadBlock(c, atoi(_SH_cmd.args[0]));
	for(i = 0; i < 512; i += 4)
	{
		unsigned long l;
		l = ((c[i] << 24) | (c[i+1] << 16) | (c[i+2] << 8) | c[i+3]);
		printf("%03x: %08x\n", i, l);
	}
	return 0;
}

static int _SH_DiskInfo(void)
{
	eFile_Info();
	return 0;
}

static int _SH_ChangeDirectory(void)
{
	if(!_SH_cmd.args[0][0])
	{
		fprintf(stderr, "Usage: %s <directory>\n", _SH_cmd.command);
		return 1;
	}
	if(eFile_ChangeDirectory(_SH_cmd.args[0]))
	{
		fprintf(stderr, "%s: No such directory\n", _SH_cmd.args[0]);
		return 1;
	}
	return 0;
}
