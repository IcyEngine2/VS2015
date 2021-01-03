/* Copyright (C) 2007 Jean-Marc Valin

   File: os_support.h
   This is the (tiny) OS abstraction layer. Aside from math.h, this is the
   only place where system headers are allowed.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef OS_SUPPORT_H
#define OS_SUPPORT_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef OS_SUPPORT_CUSTOM
#include "os_support_custom.h"
#endif

/** Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_free
    NOTE: speex_alloc needs to CLEAR THE MEMORY */
//extern void* speex_alloc(int size);

/** Same as speex_alloc, except that the area is only needed inside a Speex call (might cause problem with wideband though) */
//static inline void* speex_alloc_scratch(int size) { return speex_alloc(size); }

// Speex wrapper for realloc. To do your own dynamic allocation, all you need to do is replace this function, speex_alloc and speex_free */
//extern void* speex_realloc(void* ptr, int size);

// Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_alloc */
//extern void speex_free(void* ptr);

/** Same as speex_free, except that the area is only needed inside a Speex call (might cause problem with wideband though) */
//static inline void speex_free_scratch(void* ptr){ speex_free(ptr); }

/** Copy n elements from src to dst. The 0* term provides compile-time type checking  */
#define SPEEX_COPY(dst, src, n) (memcpy((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))

/** Copy n elements from src to dst, allowing overlapping regions. The 0* term
    provides compile-time type checking */
#define SPEEX_MOVE(dst, src, n) (memmove((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))

/** For n elements worth of memory, set every byte to the value of c, starting at address dst */
#define SPEEX_MEMSET(dst, c, n) (memset((dst), (c), (n)*sizeof(*(dst))))

static inline void _speex_fatal(const char* str, const char* file, int line) { }

static inline void speex_warning(const char* str) { }

static inline void speex_warning_int(const char* str, int val){ }

static inline void speex_notify(const char* str) { }

static inline void _speex_putc(int ch, void* file) { }

#define speex_fatal(str) _speex_fatal(str, __FILE__, __LINE__);
#define speex_assert(cond) {if (!(cond)) {speex_fatal("assertion failed: " #cond);}}

#ifndef RELEASE
static inline void print_vec(float* vec, int len, char* name);
#endif

#endif

