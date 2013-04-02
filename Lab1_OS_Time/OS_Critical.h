#ifndef __OS_CRITICAL_H__
#define __OS_CRITICAL_H__

typedef unsigned int OS_CPU_SR;

#define OS_ENTER_CRITICAL() { cpu_sr = OS_CPU_SR_Save(); }
#define OS_EXIT_CRITICAL() { OS_CPU_SR_Restore(cpu_sr); }

void OS_CPU_SR_Restore(OS_CPU_SR cps_sr);
OS_CPU_SR OS_CPU_SR_Save(void);

#endif
