#pragma once

/**
 * Cross-platform byteswap helpers taken from
 * https://github.com/OlafvdSpek/ctemplate/blob/master/src/base/macros.h
 *
 * Copyright (c) 2005, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"
#include "../config.h"


// This is all to figure out endian-ness and byte-swapping on various systems
#if defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>       // location on FreeBSD
#elif defined(HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>   // location on OS X
#endif
#if defined(HAVE_SYS_BYTEORDER_H)
#include <sys/byteorder.h>    // BSWAP_32 on Solaris 10
#endif
#ifdef HAVE_SYS_ISA_DEFS_H
#include <sys/isa_defs.h>     // _BIG_ENDIAN/_LITTLE_ENDIAN on Solaris 10
#endif

#if defined(HAVE_BYTESWAP_H)
 #include <byteswap.h>              // GNU (especially linux)
 #define BSWAP32(x)  bswap_32(x)
 #define BSWAP64(x)  bswap_64(x)
#elif defined(HAVE_LIBKERN_OSBYTEORDER_H)
 #include <libkern/OSByteOrder.h>   // OS X
 #define BSWAP32(x)  OSSwapInt32(x)
 #define BSWAP64(x)  0SSwapInt64(x)
#elif defined(bswap32)              // FreeBSD
  // FreeBSD defines bswap32 as a macro in sys/endian.h (already #included)
  #define BSWAP32(x)  bswap32(x)
  #define BSWAP64(x)  bswap64(x)
#elif defined(BSWAP_32)             // Solaris 10
  // Solaris defines BSWSAP_32 as a macro in sys/byteorder.h (already #included)
  #define BSWAP32(x)  BSWAP_32(x)
  #define BSWAP64(x)  BSWAP_64(x)
#elif !defined(BSWAP32)
#define BSWAP32(x)  ((((x) & 0x000000ff) << 24) |      \
                      (((x) & 0x0000ff00) << 8)  |      \
                      (((x) & 0x00ff0000) >> 8)  |      \
                      (((x) & 0xff000000) >> 24))
#define BSWAP64(x) ((((x) & 0x00000000000000ffULL) << 56) | \
		      (((x) & 0x000000000000ff00ULL) << 40) | \
		      (((x) & 0x0000000000ff0000ULL) << 24) | \
		      (((x) & 0x00000000ff000000ULL) <<  8) | \
	              (((x) & 0x000000ff00000000ULL) >>  8) | \
		      (((x) & 0x0000ff0000000000ULL) >> 24) | \
		      (((x) & 0x00ff000000000000ULL) >> 40) | \
		      (((x) & 0xff00000000000000ULL) >> 56) )
#else
#pragma warning "Byteswap missing !"
#endif

/* Exposed functions. */
int is_ascii_ptr(uint64_t address, arch_t arch);
int get_arch_pointer_size(arch_t arch);
uint64_t read_pointer(arch_t arch, endianness_t endian, unsigned char *p_content, unsigned int offset);
double entropy(unsigned char *p_data, int size);

void progress_bar(uint64_t current, uint64_t max, char *desc);
void progress_bar_done(void);
