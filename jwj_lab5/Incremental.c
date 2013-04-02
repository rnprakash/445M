// Incremental.c
// Runs on LM3S811
// Use a setup similar to PeriodMeasure.c to measure the
// tachometer period.  Implement an incremental controller to
// keep this period near a desired value.
// Daniel Valvano
// June 30, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011

 Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// DC motor with TIP120 interface connected to PD0 (PWM0)
// Tachometer connected to PD4 (CCP0) with 10 K pull-up
// See <paper schematic>
//
// This program drives PD0 with a particular duty cycle and measures
// the period of the tachometer signal.  The measured period of the
// tachometer signal is compared to the desired SETPOINT.  If the
// measured period is too long, then the motor is running too slowly
// and the PD0 duty cycle is increased by a constant.  Likewise, if
// the measured period is too short, then the motor is running too
// quickly and the PD0 duty cycle is decreased by a constant.  This
// is an incremental controller.  See chapter PI.c for a PI
// controller.
//
// NOTE: This program utilizes two different modules of the LM3S811
// which have different clocks.  The Timer0 module is used to
// measure the tachometer period and runs at a frequency of 6 MHz
// (the system clock frequency).  The PWM0 module is used to
// generate a variable duty cycle square wave to drive the motor and
// runs at a frequency of 3 MHz (the system clock frequency divided
// by two).

#include "PeriodMeasure.c"
//#include "PWM.c"

#define PWM_0_CMPA_R            (*((volatile unsigned long *)0x40028058))

//#define SETPOINT       60000  // (1/clockfreq) units (controller setpoint for ~100 Hz tachometer)
#define SETPOINT       48000  // (1/clockfreq) units (controller setpoint for ~125 Hz tachometer)
//#define SETPOINT       45000  // (1/clockfreq) units (controller setpoint for ~133 Hz tachometer)
//#define SETPOINT       40000  // (1/clockfreq) units (controller setpoint for ~150 Hz tachometer) (too fast)
//#define SETPOINT       36000  // (1/clockfreq) units (controller setpoint for ~167 Hz tachometer) (too fast)
#define PWMPERIOD       3000  // (1/(2*clockfreq)) units (for 1,000 Hz PWM)
#define INCREMENT          2  // (1/(2*clockfreq)) units

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
// initialize Timer0 for period measurement (see PeriodMeasure.c)
void PeriodMeasure_Init(void);
// initialize hardware PWM0 (see PWM.c)
void PWM0_Init(unsigned short period, unsigned short duty);
// set hardware PWM0 duty cycle (see PWMSine.c)
// newDuty is number of PWM clock cycles before output changes (2<=newDuty<=period-1)
// If PWM0_Init() was called, the output goes from 0 to 1 when the
// PWM counter reaches 'newDuty'.  Regardless, PWM0 must have
// already been configured before calling this function.
void PWM0_SetDuty(unsigned short newDuty){
  PWM_0_CMPA_R = newDuty - 1;          // count value when output may change
}

int main(void){
  unsigned short currentDuty = PWMPERIOD/2;// (1/(2*clockfreq)) units
                                       // initialize PWM0
                                       // 6,000,000/2/PWMPERIOD Hz
  PWM0_Init(PWMPERIOD, currentDuty);   // PWMPERIOD/currentDuty*100 % duty
  PeriodMeasure_Init();                // initialize timer0A in capture mode
  while(1){
    while(Done == 0){};                // wait for measurement
    Done = 0;
    if((Period > SETPOINT) && ((currentDuty + INCREMENT) <= (PWMPERIOD - 1))){
      // motor is running too slowly and is able to be sped up
      currentDuty = currentDuty + INCREMENT;
      PWM0_SetDuty(currentDuty);
    }
    else if((Period < SETPOINT) && ((currentDuty - INCREMENT) >= 2)){
      // motor is running too quickly and is able to be slowed down
      currentDuty = currentDuty - INCREMENT;
      PWM0_SetDuty(currentDuty);
    }
  }
}
