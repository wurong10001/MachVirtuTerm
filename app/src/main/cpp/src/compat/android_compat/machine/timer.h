/*
 * android_compat/machine/timer.h
 *
 * PA-RISC (hp_pa) equivalent of the per-architecture timer header.
 * The original MkLinux tree only ships i386/timer.h; for the HP700 target
 * we use the same 1 MHz tick model as the generic STAT_TIME fallback.
 */

#ifndef _ANDROID_COMPAT_HP_PA_TIMER_H_
#define _ANDROID_COMPAT_HP_PA_TIMER_H_

#define TIMER_RATE	1000000
#define TIMER_HIGH_UNIT	TIMER_RATE
#undef	TIMER_ADJUST

#endif	/* _ANDROID_COMPAT_HP_PA_TIMER_H_ */
