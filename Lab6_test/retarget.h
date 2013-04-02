#ifndef __RETARGET_H__

void RT_StreamToFile(int st);
__attribute__((long_call, section(".data"))) int printf(const char* format, ...);

#endif
