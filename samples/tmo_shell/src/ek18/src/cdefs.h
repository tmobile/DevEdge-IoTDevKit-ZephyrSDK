#ifndef __CDEFS_H__
#define __CDEFS_H__

/*
  By default, the internal routines of kermit.c are not static,
  because this is not allowed in some embedded environments.
  To have them declared static, define STATIC=static on the cc
  command line.
*/
#ifdef XAC  /* HiTech's XAC cmd line is small */
#define STATIC static
#else /* XAC */
#ifndef STATIC
#define STATIC
#endif /* STATIC */
#endif	/* XAC */

/*
  By default we assume the compiler supports unsigned char and
  unsigned long.  If not you can override these definitions on
  the cc command line.
*/
#ifndef HAVE_UCHAR
typedef unsigned char UCHAR;
#endif /* HAVE_UCHARE */
#ifndef HAVE_ULONG
typedef unsigned long ULONG;
#endif /* HAVE_ULONG */
#ifndef HAVE_USHORT
typedef unsigned short USHORT;
#endif /* HAVE_USHORT */

/* Unix platform.h for EK */

#ifndef IBUFLEN
#define IBUFLEN  2048			/* File input buffer size */
#endif /* IBUFLEN */

#ifndef OBUFLEN
#define OBUFLEN  2048                   /* File output buffer size */
#endif /* OBUFLEN */

#define P_PKTLEN 2000
#define P_WSLOTS 4

#endif /* __CDEFS_H__ */
