#include "lm3s8962.h"
#include "PeriodMeasure.h"

#define PD6 (*((volatile unsigned long *)0x40007100))


Sema4Type pinged;
void ping_Init(){
	
	PeriodMeasure_Init();
	GPIO_PORTD_DEN_R |= 0x40; //enable PD6
	GPIO_PORTD_DIR_R |= 0x40; //set PD6 to output
	PD6 = 0;
	
	OS_InitSemaphore(pinged, 0);
	
}


unsigned long ping_trigger(){int i;
	
	TIMER0_IMR_R &= ~TIMER_IMR_CAEIM; // disable capture match interrupt

	//send 10us pulse
	PD6 = 1;
	for(i = 0; i < 500; i ++){} //wait 10 us
	//enable interrupts
	First = TIMER0_TAR_R;
  TIMER0_IMR_R |= TIMER_IMR_CAEIM; // enable capture match interrupt
	//wait for echo
	OS_SpinWait(pinged);
		
		
	
}