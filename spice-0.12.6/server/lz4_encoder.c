/*
   Copyright (C) 2014 Flexible Software Solutions S.L.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_LZ4

#define SPICE_LOG_DOMAIN "SpiceLz4Encoder"

#include <arpa/inet.h>
#include <lz4.h>
#include "red_common.h"
#include "lz4_encoder.h"

typedef struct Lz4Encoder {
    Lz4EncoderUsrContext *usr;
} Lz4Encoder;

Lz4EncoderContext* lz4_encoder_create(Lz4EncoderUsrContext *usr)
{
    Lz4Encoder *enc;
    if (!usr->more_space || !usr->more_lines) {
        return NULL;
    }

    enc = spice_new0(Lz4Encoder, 1);
    enc->usr = usr;

    return (Lz4EncoderContext*)enc;
}

void lz4_encoder_destroy(Lz4EncoderContext* encoder)
{
    free(encoder);
}

int lz4_encode(Lz4EncoderContext *lz4, int height, int stride, uint8_t *io_ptr,
               unsigned int num_io_bytes, int top_down, uint8_t format)
{
    Lz4Encoder *enc = (Lz4Encoder *)lz4;
    uint8_t *lines;
    int num_lines = 0;
    int total_lines = 0;
    int in_size, enc_size, out_size, already_copied;
    uint8_t *in_buf, *compressed_lines;
    uint8_t *out_buf = io_ptr;
    LZ4_stream_t *stream = LZ4_createStream();

    // Encode direction and format
    *(out_buf++) = top_down ? 1 : 0;
    *(out_buf++) = format;
    num_io_bytes -= 2;
    out_size = 2;

    do {
        num_lines = enc->usr->more_lines(enc->usr, &lines);
        if (num_lines <= 0) {
            spice_error("more lines failed");
            LZ4_freeStream(stream);
            return 0;
        }
        in_buf = lines;
        in_size = stride * num_lines;
        lines += in_size;
        compressed_lines = (uint8_t *) malloc(LZ4_compressBound(in_size) + 4);
        enc_size = LZ4_compress_continue(stream, (const char *) in_buf,
                                         (char *) compressed_lines + 4, in_size);
        if (enc_size <= 0) {
            spice_error("compress failed!");
            free(compressed_lines);
            LZ4_freeStream(stream);
            return 0;
        }
        *((uint32_t *)compressed_lines) = htonl(enc_size);

        out_size += enc_size += 4;
        already_copied = 0;
        while (num_io_bytes < enc_size) {
            memcpy(out_buf, compressed_lines + already_copied, num_io_bytes);
            already_copied += num_io_bytes;
            enc_size -= num_io_bytes;
            num_io_bytes = enc->usr->more_space(enc->usr, &io_ptr);
            if (num_io_bytes <= 0) {
                spice_error("more space failed");
                free(compressed_lines);
                LZ4_freeStream(stream);
                return 0;
            }
            out_buf = io_ptr;
        }
        memcpy(out_buf, compressed_lines + already_copied, enc_size);
        out_buf += enc_size;
        num_io_bytes -= enc_size;

        free(compressed_lines);
        total_lines += num_lines;
    } while (total_lines < height);

    LZ4_freeStream(stream);
    if (total_lines != height) {
        spice_error("too many lines\n");
        out_size = 0;
    }

    return out_size;
}

#endif // USE_LZ4
