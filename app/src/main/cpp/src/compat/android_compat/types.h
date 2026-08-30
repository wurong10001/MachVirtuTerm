/*
 * android_compat/types.h
 *
 * Android adaptation of the Mach kernel <types.h> header.
 *
 * The original MkLinux build generates <types.h> on the fly; on Android we
 * provide an equivalent aggregated header that maps Mach kernel types onto
 * the C99 <stdint.h> universe while keeping the Mach type names intact
 * (boolean_t, vm_offset_t, u_int8_t, quad_t, ...).
 */

#ifndef _ANDROID_COMPAT_TYPES_H_
#define _ANDROID_COMPAT_TYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Mach core types.
 */
#include <mach/boolean.h>	/* boolean_t, TRUE, FALSE */
#include <mach/kern_return.h>	/* kern_return_t, KERN_* */
#include <mach/port.h>		/* mach_port_t */
#include <mach/vm_types.h>	/* vm_offset_t, vm_size_t, natural_t, ... */
#include <mach/vm_prot.h>	/* vm_prot_t */
#include <ipc/ipc_types.h>	/* ipc_port_t, ipc_space_t */

/*
 * Legacy BSD-style shorthand types used pervasively by Mach 3.0 device
 * drivers (HP700 layer in particular).
 */
typedef unsigned char	u_char;		/* unsigned char */
typedef unsigned short	u_short;	/* unsigned short */
typedef unsigned int	u_int;		/* unsigned int */
typedef unsigned long	u_long;		/* unsigned long */

/*
 * Fixed-width u_* types (documented in the migration plan: map onto
 * <stdint.h>).
 */
typedef uint8_t		u_int8_t;
typedef uint16_t	u_int16_t;
typedef uint32_t	u_int32_t;
typedef uint64_t	u_int64_t;

/* 64-bit quantity (HP PA-RISC "quad") */
typedef long long	quad_t;
typedef unsigned long long u_quad_t;

typedef char *		caddr_t;	/* address of a (signed) char */

typedef int		time_t;		/* a signed 32-bit time */
typedef unsigned int	daddr_t;	/* an unsigned 32-bit disk address */
typedef unsigned int	off_t;		/* another unsigned 32-bit offset */

typedef unsigned short	dev_t;		/* another unsigned short */
#define	NODEV		((dev_t)-1)	/* and a null value for it */

#define	major(i)	(((i) >> 8) & 0xFF)
#define	minor(i)	((i) & 0xFF)
#define	makedev(i,j)	((((i) & 0xFF) << 8) | ((j) & 0xFF))

#define	NBBY		8

#ifndef	NULL
#define	NULL		((void *) 0)	/* the null pointer */
#endif

/*
 * Shorthand type definitions for unsigned storage classes
 */
typedef unsigned char	uchar_t;
typedef unsigned short	ushort_t;
typedef unsigned int	uint_t;
typedef unsigned long	ulong_t;
typedef volatile unsigned char	vuchar_t;
typedef volatile unsigned short	vushort_t;
typedef volatile unsigned int	vuint_t;
typedef volatile unsigned long	vulong_t;

typedef uchar_t		uchar;
typedef ushort_t	ushort;
typedef uint_t		uint;
typedef ulong_t		ulong;

#endif	/* _ANDROID_COMPAT_TYPES_H_ */
