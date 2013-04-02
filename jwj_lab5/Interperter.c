#include "lm3s8962.h"
#include "eFile.h"
#include "UART2.h"
#include <string.h>
#include <stdio.h>
#define MAXSTRLEN 30



int process_cmd(char *input){
// static int screen1Line;
// unsigned short adc_val;
// char *strptr;
// char inString1[MAXSTRLEN];
// char inString2[MAXSTRLEN];
// int i;
unsigned char outVal;

  

 
//   1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork

 if(strncmp(input, "initFileSystem", 14) == 0){
  eFile_Init();
	 return 1;

 }
  
  if(strncmp(input, "formatDisk", 10) == 0){
   //format disk
   if(eFile_Format())  printf("Error: efile_format");
    return 1; 
  }
  
  if(strncmp(input, "printDirectory", 14) == 0){
   //output disk directory
	 UART_OutChar('\n');
	 UART_OutChar('\r');	
   eFile_Directory(UART_OutChar);
   return 1;
  }
  
  if(strncmp(input, "printFile ", 9) == 0){
		UART_OutChar('\n');
		UART_OutChar('\r');
   eFile_ROpen(input + 10);
   while(eFile_ReadNext(&outVal) == 0)
		 { 
			 UART_OutChar(outVal);
		 }
	 eFile_RClose();
   return 1;
  }
  
  if(strncmp(input, "deleteFile ", 11) == 0){
   eFile_Delete(input + 11);
   return 1;
  }
  
  if(strncmp(input, "redirect ", 9) == 0){
    eFile_RedirectToFile(input + 9);
	return 1;
  }   
  
  return 0;
  
  
  

}





void Interpreter(void){char inchar;

  char inString1[MAXSTRLEN];

 for(;;){
  UART_InString(inString1, MAXSTRLEN);
  
    process_cmd(inString1);
  if(StreamToFile){
		UART_OutChar('\n');
		UART_OutChar('\r');
		
    inchar = UART_InChar();
		UART_OutChar(inchar);
	
   while(inchar != 0x1B){ //while user doesn't press escape
    eFile_Write(inchar);
		inchar = UART_InChar();
		 UART_OutChar(inchar);
   }
    eFile_EndRedirectToFile();
  }
  UART_OutChar(CR);
    UART_OutChar(LF);
 }
  }
