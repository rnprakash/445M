// main program for testing the ADC driver

#include "ADC.h"

GPIO_PORTF_DATA_R

unsigned short result = 0;
unsigned short buffer[3][64];
int main (void) {
  unsigned int fs = 10000;
  ADC_Init(fs);
  ADC_Open(0);
  ADC_Open(1);
  ADC_Open(2);
  ADC_Open(3);
  result = ADC_In(0);
  ADC_Collect(0, fs, buffer[0], 64);
  ADC_Collect(1, fs, buffer[1], 64);
  ADC_Collect(2, fs, buffer[2], 64);
  ADC_Collect(3, fs, buffer[3], 64);
  while(1) {
    ;
  }
}
