#ifndef PORTABLE_H
#define PORTABLE_H

#include <stdint.h>

#if !defined(_WIN32)

#define _open  open
#define _close close
#define _read  read
#define _write write
#define _lseeki64 lseek
#define _telli64(fd)  (lseek(fd,0,SEEK_CUR))
#define _O_BINARY     (0)
#define _O_RDONLY     (O_RDONLY)
#define _O_WRONLY     (O_WRONLY)
#define _O_SEQUENTIAL (0)
#define _O_CREAT      (O_CREAT)
#define _O_TRUNC      (O_TRUNC)
#define _S_IREAD      (S_IRUSR|S_IRGRP|S_IROTH)
#define _S_IWRITE     (S_IWUSR|S_IWGRP|S_IWOTH)

#ifndef __forceinline
#define __forceinline __attribute__((always_inline))
#endif

#ifndef __restrict
#define __restrict __restrict__
#endif

#ifdef __i386__
#define _M_IX86 __i386__
#endif

#ifdef __x86_64__
#define _M_X64 __x86_64__
#define _M_AMD64 __x86_64__
#endif

#if defined(__APPLE__)

#include <libkern/OSByteOrder.h>
#define _byteswap_ulong(x)  OSSwapInt32(x)
#define _byteswap_uint64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)

#include <sys/byteorder.h>
#define _byteswap_ulong(x)  BSWAP_32(x)
#define _byteswap_uint64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)

#include <sys/endian.h>
#define _byteswap_ulong(x)  bswap32(x)
#define _byteswap_uint64(x) bswap64(x)

#elif defined(__OpenBSD__)

#include <sys/types.h>
#define _byteswap_ulong(x)  swap32(x)
#define _byteswap_uint64(x) swap64(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>
#if defined(__BSWAP_RENAME) && !defined(_byteswap_ulong)
#define _byteswap_ulong(x)  bswap32(x)
#define _byteswap_uint64(x) bswap64(x)
#endif

#else

#include <byteswap.h>
#define _byteswap_ulong(x)  bswap_32(x)
#define _byteswap_uint64(x) bswap_64(x)

#endif /* defined(__APPLE__) */

#define mem_aligned_alloc(s) aligned_alloc(s, 32)
#define mem_aligned_free free

#define ALIGNAS(s) __attribute__((aligned(s)))

#else /* !defined(_WIN32) */

#define mem_aligned_alloc(s) _aligned_malloc(s, 32)
#define mem_aligned_free _aligned_free

#define ALIGNAS(s) __declspec(align(s))

#endif /* !defined(_WIN32) */

#endif /* PORTABLE_H */
