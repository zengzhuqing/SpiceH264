/*
   Copyright (C) 2014 Flexible Software Solutions S.L.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in
         the documentation and/or other materials provided with the
         distribution.
       * Neither the name of the copyright holder nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _H_LZ4_ENCODER
#define _H_LZ4_ENCODER

#include <spice/types.h>

typedef void* Lz4EncoderContext;
typedef struct Lz4EncoderUsrContext Lz4EncoderUsrContext;

struct Lz4EncoderUsrContext {
    int (*more_space)(Lz4EncoderUsrContext *usr, uint8_t **io_ptr);
    int (*more_lines)(Lz4EncoderUsrContext *usr, uint8_t **lines);
};

Lz4EncoderContext* lz4_encoder_create(Lz4EncoderUsrContext *usr);
void lz4_encoder_destroy(Lz4EncoderContext *encoder);

/* returns the total size of the encoded data. */
int lz4_encode(Lz4EncoderContext *lz4, int height, int stride, uint8_t *io_ptr,
               unsigned int num_io_bytes, int top_down, uint8_t format);
#endif
