/**************************************************
* ADC.c 
* Runs on LM3S8962
* John Jacobellis and Nikki Verreddigari
* 1/25/2013
* This driver initializes Timer0A to trigger ADC
* conversions and request an interrupt when the 
* conversion is complete.
* It also allows an user to choose the sampling rate
* of the ADC from 100 to 10000 Hz and the ADC
* channels.
*
****************************************************/
#include "Fifo.h"
#include "lm3s8962.h"
#include "OS.h"


typedef unsigned short ADCDATATYPE;
#define ADCFIFOSIZE 32
#define ADCFIFOSUCCESS 1
#define ADCFIFOFAIL 0

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void (*Handler_Function)(ADCDATATYPE data);


AddPointerFifo(ADC1,ADCFIFOSIZE,ADCDATATYPE,ADCFIFOSUCCESS,ADCFIFOFAIL);
AddPointerFifo(ADC2,ADCFIFOSIZE,ADCDATATYPE,ADCFIFOSUCCESS,ADCFIFOFAIL);



//stupid function that we need and its stupid
void ADC_Open(){	volatile unsigned long delay;

	SYSCTL_RCGC0_R |= SYSCTL_RCGC0_ADC;      //enabling the ADC Clock by setting bit 16. SYSCTL_RCGC0_R = 0x10000
  SYSCTL_RCGC0_R &= ~SYSCTL_RCGC0_ADCSPD_M; // clear ADC sample speed field
  SYSCTL_RCGC0_R += SYSCTL_RCGC0_ADCSPD125K;// configure for 125K ADC max sample rate (default setting)
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER0;    // activate timer0
  delay = SYSCTL_RCGC1_R;     
	Handler_Function = 0;
	ADC1Fifo_Init();
	ADC2Fifo_Init();


	
}


/*******************ADC_Init****************************
ADC_Init initalizes the ADC and Timer0A to trigger 
ADC conversions
Inputs: none
Outputs: none

********************************************************/
void ADC3_init_Timer0A(unsigned char channelNum, unsigned short period){
	  if(channelNum > 3){
    return;                                 // invalid input, do nothing
  }
	OS_DisableInterrupts();
	
	/**********general initialization*********/
              // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN;          // disable timer0A during setup
  TIMER0_CTL_R |= TIMER_CTL_TAOTE;          // enable timer0A trigger to ADC
  TIMER0_CFG_R = TIMER_CFG_16_BIT;          // configure for 16-bit timer mode
  // **** timer0A initialization ****
  TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;   // configure for periodic mode
  TIMER0_TAPR_R = 49;                       // 1 microsecond per tick 
  TIMER0_TAILR_R = period - 1;                  // start value for trigger
  TIMER0_IMR_R &= ~TIMER_IMR_TATOIM;        // disable timeout (rollover) interrupt
  TIMER0_CTL_R |= TIMER_CTL_TAEN;           // enable timer0A 16-b, periodic, no interrupts
  // **** ADC initialization ****
                                            // sequencer 0 is highest priority (default setting)
                                            // sequencer 1 is second-highest priority (default setting)
                                            // sequencer 2 is third-highest priority (default setting)
                                            // sequencer 3 is lowest priority (default setting)
  ADC0_SSPRI_R = (ADC_SSPRI_SS0_1ST|ADC_SSPRI_SS1_2ND|ADC_SSPRI_SS2_3RD|ADC_SSPRI_SS3_4TH);
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN3;          // disable sample sequencer 3
  ADC0_EMUX_R &= ~ADC_EMUX_EM3_M;           // clear SS3 trigger select field
  ADC0_EMUX_R += ADC_EMUX_EM3_TIMER;        // configure for timer trigger event
  ADC0_SSMUX3_R &= ~ADC_SSMUX3_MUX0_M;      // clear SS3 1st sample input select field
                                            // configure for 'channelNum' as first sample input
  ADC0_SSMUX3_R += (channelNum<<ADC_SSMUX3_MUX0_S);
  ADC0_SSCTL3_R = (0                        // settings for 1st sample:
                   & ~ADC_SSCTL3_TS0        // read pin specified by ADC0_SSMUX3_R (default setting)
                   | ADC_SSCTL3_IE0         // raw interrupt asserted here
                   | ADC_SSCTL3_END0        // sample is end of sequence (default setting, hardwired)
                   & ~ADC_SSCTL3_D0);       // differential mode not used (default setting)
  ADC0_IM_R |= ADC_IM_MASK3;                // enable SS3 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN3;           // enable sample sequencer 3
  // **** interrupt initialization ****
                                            // ADC3=priority 0
  NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00000000; // bits 13-15
  NVIC_EN0_R |= NVIC_EN0_INT17;             // enable interrupt 17 in NVIC
  OS_EnableInterrupts();
	
}


void ADC2_init_Timer0B(unsigned char channelNum, unsigned short period){
	if(channelNum > 3){ return; }
	OS_DisableInterrupts();
	
	/**********general initialization*********/
              // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TBEN;          // disable timer0 during setup
  TIMER0_CTL_R |= TIMER_CTL_TBOTE;          // enable timer0B trigger to ADC
  TIMER0_CFG_R = TIMER_CFG_16_BIT;          // configure for 16-bit timer mode
  // **** timer0A initialization ****
  TIMER0_TBMR_R = TIMER_TBMR_TBMR_PERIOD;   // configure for periodic mode
  TIMER0_TBPR_R = 49;                       // 1 microsecond per tick 
  TIMER0_TBILR_R = period - 1;                  // start value for trigger
  TIMER0_IMR_R &= ~TIMER_IMR_TBTOIM;        // disable timeout (rollover) interrupt
  TIMER0_CTL_R |= TIMER_CTL_TBEN;           // enable timer0B 16-b, periodic, no interrupts

  // **** ADC initialization ****
                                            // sequencer 0 is highest priority (default setting)
                                            // sequencer 1 is second-highest priority (default setting)
                                            // sequencer 2 is third-highest priority (default setting)
                                            // sequencer 3 is lowest priority (default setting)
  ADC0_SSPRI_R = (ADC_SSPRI_SS0_1ST|ADC_SSPRI_SS1_2ND|ADC_SSPRI_SS2_3RD|ADC_SSPRI_SS3_4TH);
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN2;          // disable sample sequencer 2
  ADC0_EMUX_R &= ~ADC_EMUX_EM2_M;           // clear SS2 trigger select field
  ADC0_EMUX_R += ADC_EMUX_EM2_TIMER;        // configure for timer trigger event
  ADC0_SSMUX2_R &= ~ADC_SSMUX2_MUX0_M;      // clear SS2 1st sample input select field
                                            // configure for 'channelNum' as first sample input
  ADC0_SSMUX2_R += (channelNum<<ADC_SSMUX2_MUX0_S);
  ADC0_SSCTL2_R = (0                        // settings for 1st sample:
                   & ~ADC_SSCTL2_TS0        // read pin specified by ADC0_SSMUX2_R (default setting)
                   | ADC_SSCTL2_IE0         // raw interrupt asserted here
                   | ADC_SSCTL2_END0        // sample is end of sequence (default setting, hardwired)
                   & ~ADC_SSCTL2_D0);       // differential mode not used (default setting)
  ADC0_IM_R |= ADC_IM_MASK2;                // enable SS2 interrupts
  ADC_ACTSS_R |= ADC_ACTSS_ASEN2;           // enable sample sequencer 2
  // **** interrupt initialization ****
                                            // ADC3=priority 0
  NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00000000; // bits 13-15
  NVIC_EN0_R |= NVIC_EN0_INT17;             // enable interrupt 17 in NVIC
  OS_EnableInterrupts();

}



void ADC_InitSWTriggerSeq1(unsigned char channelNum){
  // channelNum must be 0-3 (inclusive) corresponding to ADC0 through ADC3
	OS_DisableInterrupts();
  if(channelNum > 3){
    return;                                 // invalid input, do nothing
  }
  // **** ADC initialization ****
                                            // sequencer 0 is highest priority (default setting)
                                            // sequencer 1 is second-highest priority (default setting)
                                            // sequencer 2 is third-highest priority (default setting)
                                            // sequencer 3 is lowest priority (default setting)
  ADC0_SSPRI_R = (ADC_SSPRI_SS0_1ST|ADC_SSPRI_SS1_2ND|ADC_SSPRI_SS2_3RD|ADC_SSPRI_SS3_4TH);
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN1;          // disable sample sequencer 2
  ADC0_EMUX_R &= ~ADC_EMUX_EM1_M;           // clear SS2 trigger select field
  ADC0_EMUX_R += ADC_EMUX_EM1_PROCESSOR;    // configure for software trigger event (default setting)
  ADC0_SSMUX1_R &= ~ADC_SSMUX1_MUX0_M;      // clear SS3 1st sample input select field
                                            // configure for 'channelNum' as first sample input
  ADC0_SSMUX1_R += (channelNum<<ADC_SSMUX1_MUX0_S);
  ADC0_SSCTL1_R = (0                        // settings for 1st sample:
                   & ~ADC_SSCTL1_TS0        // read pin specified by ADC0_SSMUX2_R (default setting)
                   | ADC_SSCTL1_IE0         // raw interrupt asserted here
                   | ADC_SSCTL1_END0        // sample is end of sequence (default setting, hardwired)
                   & ~ADC_SSCTL1_D0);       // differential mode not used (default setting)
  ADC0_IM_R |= ADC_IM_MASK1;               // enable SS1 interrupts 
  //ADC_ACTSS_R |= ADC_ACTSS_ASEN1;           // enable sample sequencer 1
	 // **** interrupt initialization ****
                                            // ADC1=priority 0
  NVIC_PRI3_R = (NVIC_PRI3_R&0x00FFFFFF)|0x00000000; // bits 0-7
  NVIC_EN0_R |= NVIC_EN0_INT15;             // enable interrupt 15 in NVIC
  OS_EnableInterrupts();
}



void ADC1_Handler(void){ADCDATATYPE data;
	  ADC0_ISC_R = ADC_ISC_IN1;
	  data = (ADCDATATYPE) (ADC0_SSFIFO1_R&ADC_SSFIFO1_DATA_M);
    if(ADC1Fifo_Put(data) == ADCFIFOSUCCESS){
	  }
}


void ADC2_Handler(void){ADCDATATYPE data;
	  ADC0_ISC_R = ADC_ISC_IN2;             // acknowledge ADC sequence 2 completion
    data = (ADCDATATYPE) (ADC0_SSFIFO2_R&ADC_SSFIFO2_DATA_M);
		if(ADC2Fifo_Put(data) == ADCFIFOSUCCESS){
	}
}

void ADC3_Handler(void){ ADCDATATYPE data;
  ADC0_ISC_R = ADC_ISC_IN3;             // acknowledge ADC sequence 3 completion
  data = (ADCDATATYPE) (ADC0_SSFIFO3_R&ADC_SSFIFO3_DATA_M);
	if(Handler_Function != 0){ //if the handler function has been defined
  (*Handler_Function)(data);
	}
}




/*******************ADC_In****************************
//1) init 2) enable 3)sample 4) disable
Inputs: channelNum
Outputs: none

********************************************************/
unsigned short ADC_In (unsigned int channelNum){

	unsigned short ADC_In_Ret_Value = 0;

		if(channelNum > 3){return 0xFFFF;}

	ADC_InitSWTriggerSeq1((char) channelNum);
	 ADC_ACTSS_R |= ADC_ACTSS_ASEN1;           // enable sample sequencer 1

	
	ADC0_PSSI_R = ADC_PSSI_SS1;               // initiate SS1
	while(ADC1Fifo_Get(&ADC_In_Ret_Value)== ADCFIFOFAIL){} //wait for conversion to finish
  ADC_ACTSS_R &= ~ADC_ACTSS_ASEN2;          // disable sample sequencer 1
	

  return ADC_In_Ret_Value; 
	
	
}

/*******************ADC_Collect****************************
ADC_Collect allows the user to choose a channel number,
the frequency of the sample rate, and the number of samples to choose
and where to store them
ADC conversions
Inputs: unsigned int channelNum, unsigned int fs, unsigned short buffer[],
                unsigned int nummberOfSamples
Outputs: none

********************************************************/

int ADC_Collect_finite(unsigned int channelNum, unsigned int fs, unsigned short buffer[],
                unsigned int numberOfSamples){


 int periodValue = 0;
 int count = 0;
 ADCDATATYPE *ptr;
 if(fs > 10000){return 0;}
 if(fs < 100){return 0;}
 if(channelNum > 3){return 0;} 

 ptr = buffer;					         // *ptr = &buffer[0]				
 periodValue = 1000000 /fs;
 
 ADC2_init_Timer0B(channelNum, periodValue);			
 ADC_ACTSS_R |= ADC_ACTSS_ASEN2;           // enable sample sequencer 2

 while(count < numberOfSamples){
	

 if (ADC2Fifo_Get(ptr) == ADCFIFOSUCCESS){
  ptr++;
	count++;
 }

 
 }									
	ADC_ACTSS_R &= ~ADC_ACTSS_ASEN2;          // disable sample sequencer 3

	return 1;

}


void ADC_Collect(int channelNum, int fs, void (*Producer)(ADCDATATYPE data)){ int periodValue = 0;

 //if(fs > 10000){return;}
 if(fs < 100){return;}
 if(channelNum > 3){return;}
 periodValue = 1000000 /fs;

 Handler_Function = Producer;
 ADC3_init_Timer0A(channelNum, periodValue);			
 ADC_ACTSS_R |= ADC_ACTSS_ASEN3;           // enable sample sequencer 3

	
}

