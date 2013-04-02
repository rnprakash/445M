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


typedef unsigned short ADCDATATYPE;




void ADC_Open(void);

/*******************ADC_Init****************************
ADC_Init initalizes the ADC and Timer0A to trigger 
ADC conversions
Inputs: none
Outputs: none

********************************************************/
void ADC_init_timer0A(void);

//initializes to SW trigger
void ADC_InitSWTriggerSeq3(unsigned char channelNum);

/*******************ADC_In****************************
//1) init 2) enable 3)sample 4) disable
Inputs: channelNum
Outputs: none

********************************************************/
unsigned short ADC_In (unsigned int channelNum);



/*******************ADC_Collect****************************
ADC_Collect allows the user to choose a channel number,
the frequency of the sample rate, and the number of samples to choose
and where to store them
ADC conversions
Inputs: unsigned int channelNum, unsigned int fs, unsigned short buffer[],
                unsigned int nummberOfSamples
Outputs: none

********************************************************/
int ADC_Collect_finite(unsigned int channelNum, unsigned int fs, unsigned short 
               buffer[], unsigned int nummberOfSamples);
								
								
//****************ADC_Custom_Collect***************************
//Runs the ADC indefinitely on a given channel number and frequency
//the Producer function gets called for every new value of the ADC
//Must call ADC_Init() first
void ADC_Collect(int channelNum, int fs, void(*Producer)(ADCDATATYPE data));



	

