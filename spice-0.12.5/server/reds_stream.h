/*
   Copyright (C) 2009, 2013 Red Hat, Inc.

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

#ifndef _H_REDS_STREAM
#define _H_REDS_STREAM

#include "spice.h"
#include "common/mem.h"

#include <stdbool.h>

#include <openssl/ssl.h>

typedef void (*AsyncReadDone)(void *opaque);
typedef void (*AsyncReadError)(void *opaque, int err);

typedef struct RedsStream RedsStream;
typedef struct RedsStreamPrivate RedsStreamPrivate;

struct RedsStream {
    int socket;
    SpiceWatch *watch;

    /* set it to TRUE if you shutdown the socket. shutdown read doesn't work as accepted -
       receive may return data afterward. check the flag before calling receive*/
    int shutdown;

    RedsStreamPrivate *priv;
};

typedef enum {
    REDS_STREAM_SSL_STATUS_OK,
    REDS_STREAM_SSL_STATUS_ERROR,
    REDS_STREAM_SSL_STATUS_WAIT_FOR_READ,
    REDS_STREAM_SSL_STATUS_WAIT_FOR_WRITE
} RedsStreamSslStatus;

/* any thread */
ssize_t reds_stream_read(RedsStream *s, void *buf, size_t nbyte);
void reds_stream_async_read(RedsStream *stream, uint8_t *data, size_t size,
                            AsyncReadDone read_done_cb, void *opaque);
void reds_stream_set_async_error_handler(RedsStream *stream,
                                         AsyncReadError error_handler);
ssize_t reds_stream_write(RedsStream *s, const void *buf, size_t nbyte);
ssize_t reds_stream_writev(RedsStream *s, const struct iovec *iov, int iovcnt);
bool reds_stream_write_all(RedsStream *stream, const void *in_buf, size_t n);
bool reds_stream_write_u8(RedsStream *s, uint8_t n);
bool reds_stream_write_u32(RedsStream *s, uint32_t n);
void reds_stream_disable_writev(RedsStream *stream);
void reds_stream_free(RedsStream *s);

void reds_stream_push_channel_event(RedsStream *s, int event);
void reds_stream_remove_watch(RedsStream* s);
void reds_stream_set_channel(RedsStream *stream, int connection_id,
                             int channel_type, int channel_id);
RedsStream *reds_stream_new(int socket);
bool reds_stream_is_ssl(RedsStream *stream);
RedsStreamSslStatus reds_stream_ssl_accept(RedsStream *stream);
int reds_stream_enable_ssl(RedsStream *stream, SSL_CTX *ctx);
void reds_stream_set_info_flag(RedsStream *stream, unsigned int flag);

typedef enum {
    REDS_SASL_ERROR_OK,
    REDS_SASL_ERROR_GENERIC,
    REDS_SASL_ERROR_INVALID_DATA,
    REDS_SASL_ERROR_RETRY,
    REDS_SASL_ERROR_CONTINUE,
    REDS_SASL_ERROR_AUTH_FAILED
} RedsSaslError;

RedsSaslError reds_sasl_handle_auth_step(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
RedsSaslError reds_sasl_handle_auth_steplen(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
RedsSaslError reds_sasl_handle_auth_start(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
RedsSaslError reds_sasl_handle_auth_startlen(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
bool reds_sasl_handle_auth_mechname(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
bool reds_sasl_handle_auth_mechlen(RedsStream *stream, AsyncReadDone read_cb, void *opaque);
bool reds_sasl_start_auth(RedsStream *stream, AsyncReadDone read_cb, void *opaque);

#endif
