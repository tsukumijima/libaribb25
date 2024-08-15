#include <stdio.h>
#include <config.h>

// See https://github.com/open62541/open62541/commit/ea258bc825e7b01bc92544ac83d8215fe0c72a07
#if defined(_WIN32)
#include <stdlib.h>
#else
#include <unistd.h>
#endif
#if defined(__GNUC__) || defined(__clang__)
#  if !defined(__APPLE__)
const char elf_interp[] __attribute__((section(".interp"))) = ELF_INTERP;
#  endif

void show_version(void)
{
	fprintf(stderr, "libaribb25.so - ARIB STD-B25 shared library version %s (%s)\n", ARIBB25_VERSION_STRING, BUILD_GIT_REVISION);
	fprintf(stderr, "  built with %s %s on %s\n", BUILD_CC_NAME, BUILD_CC_VERSION, BUILD_OS_NAME);
	_exit(0);
}

#endif
