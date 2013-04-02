// ADC.h

// should this take a channel number as the argument?
int ADC_Open(void); 

unsigned short ADC_In(unsigned int channelNum); 

int ADC_Collect(unsigned int channelNum, unsigned int fs, 

unsigned short buffer[], unsigned int numberOfSamples); 