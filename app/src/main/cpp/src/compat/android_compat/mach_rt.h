/*
 * android_compat/mach_rt.h
 *
 * Minimal stand-in for the MkLinux <mach_rt.h> "Mach runtime" header.
 * In the original tree this header pulled in the full Mach type set; here
 * we only need the VM/runtime types that the HP700 device layer relies on.
 */

#ifndef _ANDROID_COMPAT_MACH_RT_H_
#define _ANDROID_COMPAT_MACH_RT_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/vm_types.h>

#endif	/* _ANDROID_COMPAT_MACH_RT_H_ */
