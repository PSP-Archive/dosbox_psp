/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */


/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


// ----- OS ! YOU MUST CHANGE THIS FOR THE BUILD PLATFORM
/* #undef AC_APPLE_UNIVERSAL_BUILD */
/* #undef BSD */
/* #undef LINUX */
/* #undef MACOSX */
/* #undef OS2 */

#ifdef __WIN32__
# define WIN32 1
#else
//# define MACOSX 1
#endif

#if defined(__i386__) || defined(__x86_64__)
#define C_UNALIGNED_MEMORY 1 /* Define to 1 to use a unaligned memory access */
#endif

// ----- DOSBOX CORE FEATURES: Many of these probably won't work even if you enable them
#define C_FPU 1 /* Define to 1 to enable floating point emulation */
#define C_DYNREC 1
/* #undef C_CORE_INLINE */ /* Define to 1 to use inlined memory functions in cpu core */
/* #undef C_DIRECTSERIAL */ /* Define to 1 if you want serial passthrough support (Win32, Posix and OS/2). */
/* #undef C_IPX */ /* Define to 1 to enable IPX over Internet networking, requires SDL_net */
/* #undef C_MODEM */ /* Define to 1 to enable internal modem support, requires SDL_net */
/* #undef C_SDL_SOUND */ /* Define to 1 to enable SDL_sound support */

// ----- HEADERS: Define if headers exist in build environment
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#if !defined(__CELLOS_LV2__)
#define HAVE_STRINGS_H 1
#endif
#define HAVE_STRING_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

// target CPU
#define C_TARGETCPU MIPSEL


#if !defined(__WIN32__) && !defined(GEKKO) && !defined(__CELLOS_LV2__) && !defined(VITA) && !defined(_3DS) && !defined(PSP)
# define HAVE_PWD_H 1
#endif

// ----- STANDARD LIBRARY FEATURES
#if !defined (__QNX__) && !defined(VITA) && !defined(PSP)
#define DIRENT_HAS_D_TYPE 1 /* struct dirent has d_type */
#endif
/* #undef DB_HAVE_NO_POWF */ /* libm doesn't include powf */
/* #undef TM_IN_SYS_TIME */ /* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef size_t */ /* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef socklen_t */ /* Define to `int` if you don't have socklen_t */

// ----- COMPILER FEATURES
#define C_ATTRIBUTE_ALWAYS_INLINE 1
#define C_HAS_ATTRIBUTE 1
#define C_HAS_BUILTIN_EXPECT 1
/* #undef C_ATTRIBUTE_FASTCALL */



///////////////////

/* Version number of package */
#define VERSION "0.72"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

#ifndef INLINE
#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif
#endif

#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif

#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
typedef double Real64;
typedef uint8_t Bit8u;
typedef int8_t Bit8s;
typedef uint16_t Bit16u;
typedef int16_t Bit16s;
typedef uint32_t Bit32u;
typedef int32_t Bit32s;
typedef uint64_t Bit64u;
typedef int64_t Bit64s;

#define sBit32t PRId32
#define sBit64t PRId64
#define sBit32fs(a) sBit32t #a
#define sBit64fs(a) sBit64t #a
typedef uintptr_t Bitu;
typedef intptr_t Bits;
#if UINTPTR_MAX == 0xFFFFFFFF
#define sBitfs sBit32fs
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define sBitfs sBit64fs
#else
#error "Can't determine pointer size"
#endif


