#include "OS_Critical.h"

	//Keil uVision assembly code
	__asm 
	OS_CPU_SR OS_CPU_SR_Save()
	{
		MRS R0, PRIMASK
		CPSID I
		BX LR
	}
	
	__asm
	void OS_CPU_SR_Restore(OS_CPU_SR cpu_sr)
	{
		MSR PRIMASK, R0
		BX LR
	}
