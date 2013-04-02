#ifndef __SHELL_H__
#define __SHELL_H__

#define SH_MAX_LENGTH 128
#define SH_NUM_VARS 64
#define SH_ERROR -1
#define SH_NULL NULL
#define SH_PROMPT_NAME "PROMPT"
#define SH_MAX_ARGS 5
#define SH_HISTORY 16

typedef struct {
  char name[10], val[25];
	char set;
} _SH_Env_Var;

typedef struct {
	char command[11];
	char args[SH_MAX_ARGS][11];
} _SH_Command;

typedef struct {
	char command[11];
	int (*func)(void);
} _SH_CommandPtr;

void SH_Init(void);
void SH_Shell(void);
// prints jitter information
void SH_Jitter(void);
// print the event log information
char* _SH_getVar(const char* varName);
char* SH_AutoCompleteCommand(const char* start, int len);
char* SH_AutoCompleteFile(const char* start, int len);

// static functions

static _SH_Env_Var* _SH_setVar(const char* varName, const char* newVal);
static _SH_Env_Var* _SH_findVar(const char* varName);
static int _SH_Execute(void);
static void _SH_InCommand(char* in, unsigned short max_len);
static void _SH_Parse_Command(const char* input);
static void _SH_BeginRedirect(void);
static void _SH_EndRedirect(void);

static int _SH_Time(void);
static int _SH_ClearTime(void);
static int _SH_Format(void);
static int _SH_Write(void);
static int _SH_Read(void);
static int _SH_Rm(void);
static int _SH_Create(void);
static int _SH_Log(void);
static int _SH_ClearLog(void);
static int _SH_FileTest(void);
static int _SH_DirectoryList(void);
static int _SH_Editor(void);
static int _SH_Clear(void);
static int _SH_Set(void);
static int _SH_Unset(void);
static int _SH_Echo(void);
static int _SH_BinFile(void);
static int _SH_LoadBin(void);
static int _SH_HexDump(void);
static int _SH_Reset(void);
static int _SH_SectorDump(void);

#endif //__SHELL_H__
