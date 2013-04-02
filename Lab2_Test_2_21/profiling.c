#include "profiling.h"
#include "lm3s8962.h"

void toggle(int pin) {
  #if PROFILING == 1
    GPIO_PORTB_DATA_R ^= pin;
  #endif
}
