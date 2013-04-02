#include <stdio.h>
#include <rt_misc.h>
#include "UART.h"
#include "eFile.h"
#include "retarget.h"

#pragma import(__use_no_semihosting_swi)

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;
static char StreamToFile;

void RT_StreamToFile(int st)
{
	StreamToFile = st;
}

int fputc(int c, FILE *f) {
	if(StreamToFile && f != stderr)
	{
		if(eFile_Write(c)) // close file on error
		{
			eFile_EndRedirectToFile(); // cannot write to file
			return 1; // failure
		}
		return 0; // success writing
	}
	if(c == '\n')
		UART_OutChar('\r');
  UART_OutChar(c);
	return 0;
}


int fgetc(FILE *f) {
  return (UART_InChar());
}


int ferror(FILE *f) {
  /* Your implementation of ferror */
	
  return EOF;
}


void _ttywrch(int c) {
  UART_OutChar(c);
}


void _sys_exit(int return_code) {
label:  goto label;  /* endless loop */
}
