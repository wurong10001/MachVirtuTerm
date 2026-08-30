/*
 * android_compat/mach_host.h
 *
 * Minimal replacement for the missing MkLinux <mach_host.h>.  The host
 * type itself is defined by <kern/host.h>, so this header just re-exports
 * it (kern/thread.h pulls <mach_host.h> in before <kern/host.h>).
 */

#ifndef _ANDROID_COMPAT_MACH_HOST_H_
#define _ANDROID_COMPAT_MACH_HOST_H_

#ifdef	MACH_KERNEL
#include <kern/host.h>
#else	/* MACH_KERNEL */
#include <mach/port.h>
typedef mach_port_t		host_t;
#endif	/* MACH_KERNEL */

#endif	/* _ANDROID_COMPAT_MACH_HOST_H_ */
