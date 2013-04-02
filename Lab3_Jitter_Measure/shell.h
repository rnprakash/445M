#ifndef __SHELL_H__
#define __SHELL_H__

void SH_Init(void);
void SH_Shell(void);
// prints jitter information
void SH_Jitter(void);

#define SH_MAX_LENGTH 128
#define SH_NUM_VARS 64
#define SH_NL "\r\n"
#define SH_ERROR -1
#define SH_NULL NULL
#define SH_INVALID_CMD "Not a recognized command" SH_NL
#define SH_PROMPT_NAME "PROMPT"

#endif //__SHELL_H__
