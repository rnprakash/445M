//InterpreterMain.c
//simple interpreter using UART port on LM3S8962


#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include <stdio.h>
#include <cstdlib>
#include "string.h"
#include "Output.h"
#include "rit128x96x4.h"
#include "UART2.h"
#include "ADC.h"
#include "OS.h"


#define MAXSTRLEN 30

int screen1Line; //cursor locations for onboard OLED
int screen2Line;

int process_cmd(char *input){
	unsigned short adc_val;
	char *strptr;
	char inString1[MAXSTRLEN];
	char inString2[MAXSTRLEN];
	
		if(strncmp(input, "adcopen", 7) == 0){ //strncmp(str1, str2, maxlen) returns 0 if the same
			ADC_Open();
			return 1;
		}
		
		if(strncmp(input, "adcin", 5) == 0){
			adc_val = ADC_In(3);			
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutUDec((long) adc_val);
			return 1;
		}
		
		if(strncmp(input, "print ", 6) == 0){
			strptr = input + 6;  //get string value after 'print ' command
			OLED_TxtMessage (0, screen1Line, strptr);
			screen1Line = (screen1Line + 1) %4;
		 return 1;
    }
		
		if(strncmp(input, "printval", 8) == 0){
			UART_OutChar(CR);
			UART_OutChar(LF); 			
			UART_OutString("Enter name: ");
			UART_InString(inString1, MAXSTRLEN);
			UART_OutChar(CR);
			UART_OutChar(LF);
			UART_OutString("Enter value: ");
			UART_InString(inString2, MAXSTRLEN);
			OLED_Message(0, screen1Line, inString1, (long) atoi(inString2));
			
		}
		
			if(strncmp(input, "cheermeup", 9)== 0){
			OLED_TxtMessage(0, screen1Line, ":-) hang in there!");
			screen1Line = (screen1Line + 1) %4;

			return 1;
		}
	
		
		return 0;
		

}

void dummy(void){
}
int oldmain(void){
//int main(void){
//test variables
//	unsigned short testBuffer[5];
//	unsigned short return_val;

  char inString1[MAXSTRLEN];
  //
  // Set the clocking to run at 50MHz from the PLL.
  //
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_8MHZ);
	
	Output_Init();
  Output_Color(15);
	screen1Line = 0;
	screen2Line = 0;
	UART_Init();
	
	//ADC Test Code *******************************
//  	ADC_Open();
// 	return_val = ADC_In(3);
// 	return_val = ADC_In(3);
// 	ADC_Collect(3, 10000, testBuffer, 5);
// 	ADC_Collect(3, 1000, testBuffer, 5);
	//ADC Test Code *******************************
	
	
	
	//OLED Test Code********************************
// 	OLED_Message(0, 0, "line", 0);
// 	OLED_Message(0, 1, "next", 543254);
// 	OLED_Message(0, 2, "bignumbigtexthaha", 11111111);
// 	OLED_Message(0, 3, "middle", 543);
// 	OLED_Message(1, 0, "screen", 1);
// 	OLED_Message(1, 3, "last", 4349);
// 	OLED_Message(0, 0, "override::", 10101);
	//END OLEN Test Code ***************************
	
  Timer2A_Init(&dummy, 1);
	for(;;){
		
		
		//UART test code*****************************
// 	UART_InString(inString1, MAXSTRLEN);
// 	UART_OutChar(' ');
// 	UART_InString(inString2, MAXSTRLEN);
// 	inputNum = atol(inString2);
//   OLED_Message(0,0, inString1, inputNum);
// 	UART_OutChar(CR);
//   UART_OutChar(LF);
		//END UART test code*****************************

		
		
		UART_InString(inString1, MAXSTRLEN);
		
    process_cmd(inString1);
		
		UART_OutChar(CR);
    UART_OutChar(LF);
		
  }
  
}
