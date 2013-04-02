#include "ADC.h"
#include "OS.h"
#include "rit128x96x4.h"
#include "shell.h"
#include <stdio.h>

#define FS 39 // corresponds to 12.8 kHz sampling rate
#define FILTER_LEN 51
#define PLOT_TIME 0
#define PLOT_FREQ 1

void Sampler(unsigned short sample);
void FIR_Filter(void);
void Scope(void);
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);
unsigned long sqrt(unsigned long s);
void plotBar(long point);

unsigned long NumDataLost = 0;
unsigned char FilterEn = 0;
unsigned char ScopeMode = PLOT_TIME;
unsigned long VoltageLog[BUFF_LEN] = {0,};
int VLindex = 0;
long FFT_Log[64];
long FFT_LogIndex = 0;

void select(void)
{
	//OS_AddThread(&ADC_ChangeScopeMode,100,1);
  //OS_Kill();
	ADC_ChangeScopeMode();
	OS_Kill();
}

int main(void) {
  OLED_Init(15);
  OS_Init();
  ADC_Init(FS); // opens all ADC channels
  SH_Init();
  OS_MailBox_Init();
  OS_Fifo_Init(MAX_FIFO_SIZE);

  ADC_Collect(0, FS, &Sampler);

  OS_AddThread(&FIR_Filter, 64, 1);
//   OS_AddThread(&Scope, 64, 1);
  OS_AddThread(&SH_Shell, 64, 6);

	OS_AddButtonTask(&select, 1);
	
  OS_Launch(TIME_2MS);

  while(1) ;
}

void Sampler(unsigned short sample) {
  if(OS_Fifo_Put(sample) == 0) {
    NumDataLost++;
  }
}

unsigned long X[FILTER_LEN] = {0,};
unsigned long Y[FILTER_LEN] = {0,};
static const long H[FILTER_LEN] = {0,0,-1,-1,0,1,-1,-6,-8,-2,5,
     3,-11,-22,-12,11,17,-10,-46,-41,16,68,28,-125,-309,
     888,-309,-125,28,68,16,-41,-46,-10,17,11,-12,-22,-11,
     3,5,-2,-8,-6,-1,1,0,-1,-1,0,0};

long buffers[3][BUFF_LEN] = { {0,},};
long *inputBuff, *outputBuff, *unusedBuff;

void FIR_Filter(void) {
	static int index = 0;
  unsigned long sample;
  long *temp;
  
  inputBuff = buffers[0];
  outputBuff = buffers[1];
  unusedBuff = buffers[2];
	RIT128x96x4PlotClear(0, 1023, 0, 1, 2, 3);
  
  while(1) {
		int i;
		for(i = 0; i < BUFF_LEN; i++) {
      sample = OS_Fifo_Get();
      if(FilterEn) {
        int i, sum;
        X[index] = sample;
        sum = 0;
        for(i = 0; i < FILTER_LEN; i++)
          sum += H[i]*X[(index + FILTER_LEN - i)%FILTER_LEN];
        Y[index] = sum/256 + 512;
        sample = Y[index];
        index = (index + 1)%FILTER_LEN;
      }
      inputBuff[i] = sample;
		}
    // rotate buffers - possible race condition if the input can fill two buffers faster
    // than the output can go through one (unlikely)
    temp = unusedBuff;
    outputBuff = inputBuff;
    unusedBuff = outputBuff;
    inputBuff = temp;
    // Scope prints from output buffer
    OS_AddThread(&Scope, 64, 0);
  }
}

long FFT_in[64], FFT_out[64];
void Scope(void) {
  unsigned long FFT_output;
  // clear points on plot
  RIT128x96x4PlotReClear();
  if(ScopeMode == PLOT_TIME) {
    // plot voltage versus time
    // plot two Y values for every X value for a more connected look
    int i;
    for(i = 0; i < BUFF_LEN; i++) {
      if(VLindex < BUFF_LEN) {
        VoltageLog[VLindex++] = outputBuff[i];
      }
      RIT128x96x4PlotPoint(outputBuff[i]);
      if(i % 2) {
        RIT128x96x4PlotNext();
      }
    }
  }
  else {
    // plot voltage versus frequency
    // calculate using every other sample from outputBuff (throw away half the data)
    int i;
    for(i = 0; i < 64; i++)
    {
      FFT_in[i] = outputBuff[i];
    }
    cr4_fft_64_stm32(FFT_out, FFT_in, 64);
    for(i = 0; i < 64; i++)
    {
      long real, complex;
      real = FFT_out[i]&0xFFFF;
      complex = (FFT_out[i]&0xFFFF0000) >> 16;
      FFT_output = sqrt(real*real + complex*complex);
      if(FFT_LogIndex < 64) {
        FFT_Log[FFT_LogIndex++] = FFT_output;
      }
      plotBar(FFT_output);
//       RIT128x96x4PlotPoint(FFT_out[i]);
//       RIT128x96x4PlotNext();
    }
  }
  RIT128x96x4ShowPlot();
  OS_Kill();
}

void plotBar(long point) {
  int i;
  point *= 10;
  if(point < 0) {
    point = 0;
  }
  else if(point > 1023) {
    point = 1023;
  }
  for(i = 0; i <= point; i++) {
    RIT128x96x4PlotPoint(i);
  }
  RIT128x96x4PlotNext();  
}
