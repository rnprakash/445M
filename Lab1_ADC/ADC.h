// ADC.h

// to use the ADC:
// first call ADC_Init(fs) where fs the desired sampling rate in Hertz
// then call ADC_Open(channelNum) for each channel you wish to open
// note that all channels will operate at the same sampling rate.
// to get data, call ADC_In(channelNum) for one result or ADC_Collect() for more samples.
// ADC_Collect also sets/changes the sampling rate.

void ADC_Init(unsigned int fs);

void ADC_TimerInit(unsigned int fs);

// should this take a channel number as the argument?
int ADC_Open(int channelNum); 

unsigned short ADC_In(unsigned int channelNum); 

int ADC_Collect(unsigned int channelNum, unsigned int fs,
       unsigned short buffer[], unsigned int numberOfSamples);
