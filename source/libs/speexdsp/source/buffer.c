/* Copyright (C) 2007 Jean-Marc Valin

   File: buffer.c
   This is a very simple ring buffer implementation. It is not thread-safe
   so you need to do your own locking.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "os_support.h"
#include "arch.h"
#include "../include/speex_buffer.h"

struct SpeexBuffer_ {
   char *data;
   int   size;
   int   read_ptr;
   int   write_ptr;
   int   available;
   speex_realloc_fn realloc;
   void* user;
};

SpeexBuffer *speex_buffer_init(int size, speex_realloc_fn realloc, void* user)
{
   SpeexBuffer *st = realloc(0, sizeof(SpeexBuffer), user);
   if (st)
   {
        st->realloc = realloc;
        st->user = user;
        st->data = realloc(0, size, user);
        if (!st->data)
        {
            realloc(st, 0, user);
            return 0;
        }
        st->size = size;
        st->read_ptr = 0;
        st->write_ptr = 0;
        st->available = 0;
   }
   return st;
}

void speex_buffer_destroy(SpeexBuffer *st)
{
    st->realloc(st->data, 0, st->user);
    st->realloc(st, 0, st->user);
}

int speex_buffer_write(SpeexBuffer *st, void *_data, int len)
{
   int end;
   int end1;
   char *data = _data;
   if (len > st->size)
   {
      data += len-st->size;
      len = st->size;
   }
   end = st->write_ptr + len;
   end1 = end;
   if (end1 > st->size)
      end1 = st->size;
   SPEEX_COPY(st->data + st->write_ptr, data, end1 - st->write_ptr);
   if (end > st->size)
   {
      end -= st->size;
      SPEEX_COPY(st->data, data+end1 - st->write_ptr, end);
   }
   st->available += len;
   if (st->available > st->size)
   {
      st->available = st->size;
      st->read_ptr = st->write_ptr;
   }
   st->write_ptr += len;
   if (st->write_ptr > st->size)
      st->write_ptr -= st->size;
   return len;
}

int speex_buffer_writezeros(SpeexBuffer *st, int len)
{
   /* This is almost the same as for speex_buffer_write() but using
   SPEEX_MEMSET() instead of SPEEX_COPY(). Update accordingly. */
   int end;
   int end1;
   if (len > st->size)
   {
      len = st->size;
   }
   end = st->write_ptr + len;
   end1 = end;
   if (end1 > st->size)
      end1 = st->size;
   SPEEX_MEMSET(st->data + st->write_ptr, 0, end1 - st->write_ptr);
   if (end > st->size)
   {
      end -= st->size;
      SPEEX_MEMSET(st->data, 0, end);
   }
   st->available += len;
   if (st->available > st->size)
   {
      st->available = st->size;
      st->read_ptr = st->write_ptr;
   }
   st->write_ptr += len;
   if (st->write_ptr > st->size)
      st->write_ptr -= st->size;
   return len;
}

int speex_buffer_read(SpeexBuffer *st, void *_data, int len)
{
   int end, end1;
   char *data = _data;
   if (len > st->available)
   {
      SPEEX_MEMSET(data+st->available, 0, len - st->available);
      len = st->available;
   }
   end = st->read_ptr + len;
   end1 = end;
   if (end1 > st->size)
      end1 = st->size;
   SPEEX_COPY(data, st->data + st->read_ptr, end1 - st->read_ptr);

   if (end > st->size)
   {
      end -= st->size;
      SPEEX_COPY(data+end1 - st->read_ptr, st->data, end);
   }
   st->available -= len;
   st->read_ptr += len;
   if (st->read_ptr > st->size)
      st->read_ptr -= st->size;
   return len;
}

int speex_buffer_get_available(SpeexBuffer *st)
{
   return st->available;
}
