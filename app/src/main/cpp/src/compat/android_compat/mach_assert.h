/*
 * android_compat/mach_assert.h
 *
 * Minimal replacement for the missing MkLinux <mach_assert.h>.
 * The Mach kernel ships its own assert() in <kern/assert.h>; panic() and
 * assert_wait() are real functions declared by <kern/misc_protos.h> and
 * <kern/sched_prim.h>, so this header deliberately defines nothing but a
 * re-export of the kernel assert machinery.
 */

#ifndef _ANDROID_COMPAT_MACH_ASSERT_H_
#define _ANDROID_COMPAT_MACH_ASSERT_H_

#include <kern/assert.h>

#endif	/* _ANDROID_COMPAT_MACH_ASSERT_H_ */
