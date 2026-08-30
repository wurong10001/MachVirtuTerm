/*
 * android_compat/lan.h
 *
 * Hand-generated equivalent of the config-generated <lan.h>
 * (conf/HP700/config.devices: "pseudo-device lan").
 *
 * Also re-declares lan_addr[] here: in if_i596.c the extern declaration is
 * guarded by #ifdef PC596XMT_BUG, while the use site is unconditional, so a
 * non-PC596XMT_BUG build needs the declaration from the config header.
 */

#ifndef _ANDROID_COMPAT_LAN_H_
#define _ANDROID_COMPAT_LAN_H_

#define NLAN	1

extern unsigned char lan_addr[];	/* Core LAN address */

#endif	/* _ANDROID_COMPAT_LAN_H_ */
