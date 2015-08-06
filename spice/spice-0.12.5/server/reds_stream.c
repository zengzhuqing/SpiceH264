/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "main_dispatcher.h"
#include "red_common.h"
#include "reds_stream.h"
#include "common/log.h"

#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>

#include <glib.h>

#include <openssl/err.h>

struct AsyncRead {
    RedsStream *stream;
    void *opaque;
    uint8_t *now;
    uint8_t *end;
    AsyncReadDone done;
    AsyncReadError error;
};
typedef struct AsyncRead AsyncRead;

extern SpiceCoreInterface *core;

#if HAVE_SASL
#include <sasl/sasl.h>

typedef struct RedsSASL {
    sasl_conn_t *conn;

    /* If we want to negotiate an SSF layer with client */
    int wantSSF :1;
    /* If we are now running the SSF layer */
    int runSSF :1;

    /*
     * Buffering encoded data to allow more clear data
     * to be stuffed onto the output buffer
     */
    const uint8_t *encoded;
    unsigned int encodedLength;
    unsigned int encodedOffset;

    SpiceBuffer inbuffer;

    char *username;
    char *mechlist;
    char *mechname;

    /* temporary data during authentication */
    unsigned int len;
    char *data;
} RedsSASL;
#endif

struct RedsStreamPrivate {
    SSL *ssl;

#if HAVE_SASL
    RedsSASL sasl;
#endif

    AsyncRead async_read;

    /* life time of info:
     * allocated when creating RedsStream.
     * deallocated when main_dispatcher handles the SPICE_CHANNEL_EVENT_DISCONNECTED
     * event, either from same thread or by call back from main thread. */
    SpiceChannelEventInfo* info;

    ssize_t (*read)(RedsStream *s, void *buf, size_t nbyte);
    ssize_t (*write)(RedsStream *s, const void *buf, size_t nbyte);
    ssize_t (*writev)(RedsStream *s, const struct iovec *iov, int iovcnt);

};

static ssize_t stream_write_cb(RedsStream *s, const void *buf, size_t size)
{
    return write(s->socket, buf, size);
}

static ssize_t stream_writev_cb(RedsStream *s, const struct iovec *iov, int iovcnt)
{
    ssize_t ret = 0;
    do {
        int tosend;
        ssize_t n, expected = 0;
        int i;
#ifdef IOV_MAX
        tosend = MIN(iovcnt, IOV_MAX);
#else
        tosend = iovcnt;
#endif
        for (i = 0; i < tosend; i++) {
            expected += iov[i].iov_len;
        }
        n = writev(s->socket, iov, tosend);
        if (n <= expected) {
            if (n > 0)
                ret += n;
            return ret == 0 ? n : ret;
        }
        ret += n;
        iov += tosend;
        iovcnt -= tosend;
    } while(iovcnt > 0);

    return ret;
}

static ssize_t stream_read_cb(RedsStream *s, void *buf, size_t size)
{
    return read(s->socket, buf, size);
}

static ssize_t stream_ssl_write_cb(RedsStream *s, const void *buf, size_t size)
{
    int return_code;
    SPICE_GNUC_UNUSED int ssl_error;

    return_code = SSL_write(s->priv->ssl, buf, size);

    if (return_code < 0) {
        ssl_error = SSL_get_error(s->priv->ssl, return_code);
    }

    return return_code;
}

static ssize_t stream_ssl_read_cb(RedsStream *s, void *buf, size_t size)
{
    int return_code;
    SPICE_GNUC_UNUSED int ssl_error;

    return_code = SSL_read(s->priv->ssl, buf, size);

    if (return_code < 0) {
        ssl_error = SSL_get_error(s->priv->ssl, return_code);
    }

    return return_code;
}

void reds_stream_remove_watch(RedsStream* s)
{
    if (s->watch) {
        core->watch_remove(s->watch);
        s->watch = NULL;
    }
}

#if HAVE_SASL
static ssize_t reds_stream_sasl_read(RedsStream *s, uint8_t *buf, size_t nbyte);
#endif

ssize_t reds_stream_read(RedsStream *s, void *buf, size_t nbyte)
{
    ssize_t ret;

#if HAVE_SASL
    if (s->priv->sasl.conn && s->priv->sasl.runSSF) {
        ret = reds_stream_sasl_read(s, buf, nbyte);
    } else
#endif
        ret = s->priv->read(s, buf, nbyte);

    return ret;
}

bool reds_stream_write_all(RedsStream *stream, const void *in_buf, size_t n)
{
    const uint8_t *buf = (uint8_t *)in_buf;

    while (n) {
        int now = reds_stream_write(stream, buf, n);
        if (now <= 0) {
            if (now == -1 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return FALSE;
        }
        n -= now;
        buf += now;
    }
    return TRUE;
}

#if HAVE_SASL
static ssize_t reds_stream_sasl_write(RedsStream *s, const void *buf, size_t nbyte);
#endif

ssize_t reds_stream_write(RedsStream *s, const void *buf, size_t nbyte)
{
    ssize_t ret;

#if HAVE_SASL
    if (s->priv->sasl.conn && s->priv->sasl.runSSF) {
        ret = reds_stream_sasl_write(s, buf, nbyte);
    } else
#endif
        ret = s->priv->write(s, buf, nbyte);

    return ret;
}

ssize_t reds_stream_writev(RedsStream *s, const struct iovec *iov, int iovcnt)
{
    int i;
    int n;
    ssize_t ret = 0;

    if (s->priv->writev != NULL) {
        return s->priv->writev(s, iov, iovcnt);
    }

    for (i = 0; i < iovcnt; ++i) {
        n = reds_stream_write(s, iov[i].iov_base, iov[i].iov_len);
        if (n <= 0)
            return ret == 0 ? n : ret;
        ret += n;
    }

    return ret;
}

void reds_stream_free(RedsStream *s)
{
    if (!s) {
        return;
    }

    reds_stream_push_channel_event(s, SPICE_CHANNEL_EVENT_DISCONNECTED);

#if HAVE_SASL
    if (s->priv->sasl.conn) {
        s->priv->sasl.runSSF = s->priv->sasl.wantSSF = 0;
        s->priv->sasl.len = 0;
        s->priv->sasl.encodedLength = s->priv->sasl.encodedOffset = 0;
        s->priv->sasl.encoded = NULL;
        free(s->priv->sasl.mechlist);
        free(s->priv->sasl.mechname);
        s->priv->sasl.mechlist = NULL;
        sasl_dispose(&s->priv->sasl.conn);
        s->priv->sasl.conn = NULL;
    }
#endif

    if (s->priv->ssl) {
        SSL_free(s->priv->ssl);
    }

    reds_stream_remove_watch(s);
    spice_info("close socket fd %d", s->socket);
    close(s->socket);

    free(s);
}

void reds_stream_push_channel_event(RedsStream *s, int event)
{
    main_dispatcher_channel_event(event, s->priv->info);
}

static void reds_stream_set_socket(RedsStream *stream, int socket)
{
    stream->socket = socket;
    /* deprecated fields. Filling them for backward compatibility */
    stream->priv->info->llen = sizeof(stream->priv->info->laddr);
    stream->priv->info->plen = sizeof(stream->priv->info->paddr);
    getsockname(stream->socket, (struct sockaddr*)(&stream->priv->info->laddr), &stream->priv->info->llen);
    getpeername(stream->socket, (struct sockaddr*)(&stream->priv->info->paddr), &stream->priv->info->plen);

    stream->priv->info->flags |= SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT;
    stream->priv->info->llen_ext = sizeof(stream->priv->info->laddr_ext);
    stream->priv->info->plen_ext = sizeof(stream->priv->info->paddr_ext);
    getsockname(stream->socket, (struct sockaddr*)(&stream->priv->info->laddr_ext),
                &stream->priv->info->llen_ext);
    getpeername(stream->socket, (struct sockaddr*)(&stream->priv->info->paddr_ext),
                &stream->priv->info->plen_ext);
}


void reds_stream_set_channel(RedsStream *stream, int connection_id,
                             int channel_type, int channel_id)
{
    stream->priv->info->connection_id = connection_id;
    stream->priv->info->type = channel_type;
    stream->priv->info->id   = channel_id;
}

RedsStream *reds_stream_new(int socket)
{
    RedsStream *stream;

    stream = spice_malloc0(sizeof(RedsStream) + sizeof(RedsStreamPrivate));
    stream->priv = (RedsStreamPrivate *)(((char *)stream) + sizeof(RedsStream));
    stream->priv->info = spice_new0(SpiceChannelEventInfo, 1);
    reds_stream_set_socket(stream, socket);

    stream->priv->read = stream_read_cb;
    stream->priv->write = stream_write_cb;
    stream->priv->writev = stream_writev_cb;

    return stream;
}

bool reds_stream_is_ssl(RedsStream *stream)
{
    return (stream->priv->ssl != NULL);
}

void reds_stream_set_info_flag(RedsStream *stream, unsigned int flag)
{
    g_return_if_fail((flag == SPICE_CHANNEL_EVENT_FLAG_TLS)
                     || (flag == SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT));

    stream->priv->info->flags |= flag;
}

void reds_stream_disable_writev(RedsStream *stream)
{
    stream->priv->writev = NULL;
}

RedsStreamSslStatus reds_stream_ssl_accept(RedsStream *stream)
{
    int ssl_error;
    int return_code;

    return_code = SSL_accept(stream->priv->ssl);
    if (return_code == 1) {
        return REDS_STREAM_SSL_STATUS_OK;
    }

    ssl_error = SSL_get_error(stream->priv->ssl, return_code);
    if (return_code == -1 && (ssl_error == SSL_ERROR_WANT_READ ||
                              ssl_error == SSL_ERROR_WANT_WRITE)) {
        if (ssl_error == SSL_ERROR_WANT_READ) {
            return REDS_STREAM_SSL_STATUS_WAIT_FOR_READ;
        } else {
            return REDS_STREAM_SSL_STATUS_WAIT_FOR_WRITE;
        }
    }

    ERR_print_errors_fp(stderr);
    spice_warning("SSL_accept failed, error=%d", ssl_error);
    SSL_free(stream->priv->ssl);
    stream->priv->ssl = NULL;

    return REDS_STREAM_SSL_STATUS_ERROR;
}

int reds_stream_enable_ssl(RedsStream *stream, SSL_CTX *ctx)
{
    BIO *sbio;

    // Handle SSL handshaking
    if (!(sbio = BIO_new_socket(stream->socket, BIO_NOCLOSE))) {
        spice_warning("could not allocate ssl bio socket");
        return REDS_STREAM_SSL_STATUS_ERROR;
    }

    stream->priv->ssl = SSL_new(ctx);
    if (!stream->priv->ssl) {
        spice_warning("could not allocate ssl context");
        BIO_free(sbio);
        return REDS_STREAM_SSL_STATUS_ERROR;
    }

    SSL_set_bio(stream->priv->ssl, sbio, sbio);

    stream->priv->write = stream_ssl_write_cb;
    stream->priv->read = stream_ssl_read_cb;
    stream->priv->writev = NULL;

    return reds_stream_ssl_accept(stream);
}

void reds_stream_set_async_error_handler(RedsStream *stream,
                                         AsyncReadError error_handler)
{
    stream->priv->async_read.error = error_handler;
}

static inline void async_read_clear_handlers(AsyncRead *async)
{
    if (async->stream->watch) {
        reds_stream_remove_watch(async->stream);
    }
    async->stream = NULL;
}

static void async_read_handler(G_GNUC_UNUSED int fd,
                               G_GNUC_UNUSED int event,
                               void *data)
{
    AsyncRead *async = (AsyncRead *)data;

    for (;;) {
        int n = async->end - async->now;

        spice_assert(n > 0);
        n = reds_stream_read(async->stream, async->now, n);
        if (n <= 0) {
            if (n < 0) {
                switch (errno) {
                case EAGAIN:
                    if (!async->stream->watch) {
                        async->stream->watch = core->watch_add(async->stream->socket,
                                                               SPICE_WATCH_EVENT_READ,
                                                               async_read_handler, async);
                    }
                    return;
                case EINTR:
                    break;
                default:
                    async_read_clear_handlers(async);
		    if (async->error) {
                        async->error(async->opaque, errno);
		    }
                    return;
                }
            } else {
                async_read_clear_handlers(async);
		if (async->error) {
		    async->error(async->opaque, 0);
		}
                return;
            }
        } else {
            async->now += n;
            if (async->now == async->end) {
                async_read_clear_handlers(async);
                async->done(async->opaque);
                return;
            }
        }
    }
}

void reds_stream_async_read(RedsStream *stream,
                            uint8_t *data, size_t size,
                            AsyncReadDone read_done_cb,
                            void *opaque)
{
    AsyncRead *async = &stream->priv->async_read;

    g_return_if_fail(!async->stream);
    async->stream = stream;
    async->now = data;
    async->end = async->now + size;
    async->done = read_done_cb;
    async->opaque = opaque;
    async_read_handler(0, 0, async);

}

#if HAVE_SASL
bool reds_stream_write_u8(RedsStream *s, uint8_t n)
{
    return reds_stream_write_all(s, &n, sizeof(uint8_t));
}

bool reds_stream_write_u32(RedsStream *s, uint32_t n)
{
    return reds_stream_write_all(s, &n, sizeof(uint32_t));
}

static ssize_t reds_stream_sasl_write(RedsStream *s, const void *buf, size_t nbyte)
{
    ssize_t ret;

    if (!s->priv->sasl.encoded) {
        int err;
        err = sasl_encode(s->priv->sasl.conn, (char *)buf, nbyte,
                          (const char **)&s->priv->sasl.encoded,
                          &s->priv->sasl.encodedLength);
        if (err != SASL_OK) {
            spice_warning("sasl_encode error: %d", err);
            return -1;
        }

        if (s->priv->sasl.encodedLength == 0) {
            return 0;
        }

        if (!s->priv->sasl.encoded) {
            spice_warning("sasl_encode didn't return a buffer!");
            return 0;
        }

        s->priv->sasl.encodedOffset = 0;
    }

    ret = s->priv->write(s, s->priv->sasl.encoded + s->priv->sasl.encodedOffset,
                         s->priv->sasl.encodedLength - s->priv->sasl.encodedOffset);

    if (ret <= 0) {
        return ret;
    }

    s->priv->sasl.encodedOffset += ret;
    if (s->priv->sasl.encodedOffset == s->priv->sasl.encodedLength) {
        s->priv->sasl.encoded = NULL;
        s->priv->sasl.encodedOffset = s->priv->sasl.encodedLength = 0;
        return nbyte;
    }

    /* we didn't flush the encoded buffer */
    errno = EAGAIN;
    return -1;
}

static ssize_t reds_stream_sasl_read(RedsStream *s, uint8_t *buf, size_t nbyte)
{
    uint8_t encoded[4096];
    const char *decoded;
    unsigned int decodedlen;
    int err;
    int n;

    n = spice_buffer_copy(&s->priv->sasl.inbuffer, buf, nbyte);
    if (n > 0) {
        spice_buffer_remove(&s->priv->sasl.inbuffer, n);
        if (n == nbyte)
            return n;
        nbyte -= n;
        buf += n;
    }

    n = s->priv->read(s, encoded, sizeof(encoded));
    if (n <= 0) {
        return n;
    }

    err = sasl_decode(s->priv->sasl.conn,
                      (char *)encoded, n,
                      &decoded, &decodedlen);
    if (err != SASL_OK) {
        spice_warning("sasl_decode error: %d", err);
        return -1;
    }

    if (decodedlen == 0) {
        errno = EAGAIN;
        return -1;
    }

    n = MIN(nbyte, decodedlen);
    memcpy(buf, decoded, n);
    spice_buffer_append(&s->priv->sasl.inbuffer, decoded + n, decodedlen - n);
    return n;
}

static char *addr_to_string(const char *format,
                            struct sockaddr_storage *sa,
                            socklen_t salen)
{
    char *addr;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    int err;
    size_t addrlen;

    if ((err = getnameinfo((struct sockaddr *)sa, salen,
                           host, sizeof(host),
                           serv, sizeof(serv),
                           NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
        spice_warning("Cannot resolve address %d: %s",
                      err, gai_strerror(err));
        return NULL;
    }

    /* Enough for the existing format + the 2 vars we're
     * substituting in. */
    addrlen = strlen(format) + strlen(host) + strlen(serv);
    addr = spice_malloc(addrlen + 1);
    snprintf(addr, addrlen, format, host, serv);
    addr[addrlen] = '\0';

    return addr;
}

static char *reds_stream_get_local_address(RedsStream *stream)
{
    return addr_to_string("%s;%s", &stream->priv->info->laddr_ext,
                          stream->priv->info->llen_ext);
}

static char *reds_stream_get_remote_address(RedsStream *stream)
{
    return addr_to_string("%s;%s", &stream->priv->info->paddr_ext,
                          stream->priv->info->plen_ext);
}

static int auth_sasl_check_ssf(RedsSASL *sasl, int *runSSF)
{
    const void *val;
    int err, ssf;

    *runSSF = 0;
    if (!sasl->wantSSF) {
        return 1;
    }

    err = sasl_getprop(sasl->conn, SASL_SSF, &val);
    if (err != SASL_OK) {
        return 0;
    }

    ssf = *(const int *)val;
    spice_info("negotiated an SSF of %d", ssf);
    if (ssf < 56) {
        return 0; /* 56 is good for Kerberos */
    }

    *runSSF = 1;

    /* We have a SSF that's good enough */
    return 1;
}

/*
 * Step Msg
 *
 * Input from client:
 *
 * u32 clientin-length
 * u8-array clientin-string
 *
 * Output to client:
 *
 * u32 serverout-length
 * u8-array serverout-strin
 * u8 continue
 */
#define SASL_DATA_MAX_LEN (1024 * 1024)

RedsSaslError reds_sasl_handle_auth_step(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    const char *serverout;
    unsigned int serveroutlen;
    int err;
    char *clientdata = NULL;
    RedsSASL *sasl = &stream->priv->sasl;
    uint32_t datalen = sasl->len;

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (datalen) {
        clientdata = sasl->data;
        clientdata[datalen - 1] = '\0'; /* Wire includes '\0', but make sure */
        datalen--; /* Don't count NULL byte when passing to _start() */
    }

    spice_info("Step using SASL Data %p (%d bytes)",
               clientdata, datalen);
    err = sasl_server_step(sasl->conn,
                           clientdata,
                           datalen,
                           &serverout,
                           &serveroutlen);
    if (err != SASL_OK &&
        err != SASL_CONTINUE) {
        spice_warning("sasl step failed %d (%s)",
                      err, sasl_errdetail(sasl->conn));
        return REDS_SASL_ERROR_GENERIC;
    }

    if (serveroutlen > SASL_DATA_MAX_LEN) {
        spice_warning("sasl step reply data too long %d",
                      serveroutlen);
        return REDS_SASL_ERROR_INVALID_DATA;
    }

    spice_info("SASL return data %d bytes, %p", serveroutlen, serverout);

    if (serveroutlen) {
        serveroutlen += 1;
        reds_stream_write_all(stream, &serveroutlen, sizeof(uint32_t));
        reds_stream_write_all(stream, serverout, serveroutlen);
    } else {
        reds_stream_write_all(stream, &serveroutlen, sizeof(uint32_t));
    }

    /* Whether auth is complete */
    reds_stream_write_u8(stream, err == SASL_CONTINUE ? 0 : 1);

    if (err == SASL_CONTINUE) {
        spice_info("%s", "Authentication must continue (step)");
        /* Wait for step length */
        reds_stream_async_read(stream, (uint8_t *)&sasl->len, sizeof(uint32_t),
                               read_cb, opaque);
        return REDS_SASL_ERROR_CONTINUE;
    } else {
        int ssf;

        if (auth_sasl_check_ssf(sasl, &ssf) == 0) {
            spice_warning("Authentication rejected for weak SSF");
            goto authreject;
        }

        spice_info("Authentication successful");
        reds_stream_write_u32(stream, SPICE_LINK_ERR_OK); /* Accept auth */

        /*
         * Delay writing in SSF encoded until now
         */
        sasl->runSSF = ssf;
        reds_stream_disable_writev(stream); /* make sure writev isn't called directly anymore */

        return REDS_SASL_ERROR_OK;
    }

authreject:
    reds_stream_write_u32(stream, 1); /* Reject auth */
    reds_stream_write_u32(stream, sizeof("Authentication failed"));
    reds_stream_write_all(stream, "Authentication failed", sizeof("Authentication failed"));

    return REDS_SASL_ERROR_AUTH_FAILED;
}

RedsSaslError reds_sasl_handle_auth_steplen(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    RedsSASL *sasl = &stream->priv->sasl;

    spice_info("Got steplen %d", sasl->len);
    if (sasl->len > SASL_DATA_MAX_LEN) {
        spice_warning("Too much SASL data %d", sasl->len);
        return REDS_SASL_ERROR_INVALID_DATA;
    }

    if (sasl->len == 0) {
        read_cb(opaque);
        /* FIXME: can't report potential errors correctly here,
         * but read_cb() will have done the needed RedLinkInfo cleanups
         * if an error occurs, so the caller should not need to do more
         * treatment */
        return REDS_SASL_ERROR_OK;
    } else {
        sasl->data = spice_realloc(sasl->data, sasl->len);
        reds_stream_async_read(stream, (uint8_t *)sasl->data, sasl->len,
                               read_cb, opaque);
        return REDS_SASL_ERROR_OK;
    }
}

/*
 * Start Msg
 *
 * Input from client:
 *
 * u32 clientin-length
 * u8-array clientin-string
 *
 * Output to client:
 *
 * u32 serverout-length
 * u8-array serverout-strin
 * u8 continue
 */

RedsSaslError reds_sasl_handle_auth_start(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    const char *serverout;
    unsigned int serveroutlen;
    int err;
    char *clientdata = NULL;
    RedsSASL *sasl = &stream->priv->sasl;
    uint32_t datalen = sasl->len;

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (datalen) {
        clientdata = sasl->data;
        clientdata[datalen - 1] = '\0'; /* Should be on wire, but make sure */
        datalen--; /* Don't count NULL byte when passing to _start() */
    }

    spice_info("Start SASL auth with mechanism %s. Data %p (%d bytes)",
               sasl->mechlist, clientdata, datalen);
    err = sasl_server_start(sasl->conn,
                            sasl->mechlist,
                            clientdata,
                            datalen,
                            &serverout,
                            &serveroutlen);
    if (err != SASL_OK &&
        err != SASL_CONTINUE) {
        spice_warning("sasl start failed %d (%s)",
                    err, sasl_errdetail(sasl->conn));
        return REDS_SASL_ERROR_INVALID_DATA;
    }

    if (serveroutlen > SASL_DATA_MAX_LEN) {
        spice_warning("sasl start reply data too long %d",
                    serveroutlen);
        return REDS_SASL_ERROR_INVALID_DATA;
    }

    spice_info("SASL return data %d bytes, %p", serveroutlen, serverout);

    if (serveroutlen) {
        serveroutlen += 1;
        reds_stream_write_all(stream, &serveroutlen, sizeof(uint32_t));
        reds_stream_write_all(stream, serverout, serveroutlen);
    } else {
        reds_stream_write_all(stream, &serveroutlen, sizeof(uint32_t));
    }

    /* Whether auth is complete */
    reds_stream_write_u8(stream, err == SASL_CONTINUE ? 0 : 1);

    if (err == SASL_CONTINUE) {
        spice_info("%s", "Authentication must continue (start)");
        /* Wait for step length */
        reds_stream_async_read(stream, (uint8_t *)&sasl->len, sizeof(uint32_t),
                               read_cb, opaque);
        return REDS_SASL_ERROR_CONTINUE;
    } else {
        int ssf;

        if (auth_sasl_check_ssf(sasl, &ssf) == 0) {
            spice_warning("Authentication rejected for weak SSF");
            goto authreject;
        }

        spice_info("Authentication successful");
        reds_stream_write_u32(stream, SPICE_LINK_ERR_OK); /* Accept auth */

        /*
         * Delay writing in SSF encoded until now
         */
        sasl->runSSF = ssf;
        reds_stream_disable_writev(stream); /* make sure writev isn't called directly anymore */
        return REDS_SASL_ERROR_OK;
    }

authreject:
    reds_stream_write_u32(stream, 1); /* Reject auth */
    reds_stream_write_u32(stream, sizeof("Authentication failed"));
    reds_stream_write_all(stream, "Authentication failed", sizeof("Authentication failed"));

    return REDS_SASL_ERROR_AUTH_FAILED;
}

RedsSaslError reds_sasl_handle_auth_startlen(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    RedsSASL *sasl = &stream->priv->sasl;

    spice_info("Got client start len %d", sasl->len);
    if (sasl->len > SASL_DATA_MAX_LEN) {
        spice_warning("Too much SASL data %d", sasl->len);
        return REDS_SASL_ERROR_INVALID_DATA;
    }

    if (sasl->len == 0) {
        return REDS_SASL_ERROR_RETRY;
    }

    sasl->data = spice_realloc(sasl->data, sasl->len);
    reds_stream_async_read(stream, (uint8_t *)sasl->data, sasl->len,
                           read_cb, opaque);

    return REDS_SASL_ERROR_OK;
}

bool reds_sasl_handle_auth_mechname(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    RedsSASL *sasl = &stream->priv->sasl;

    sasl->mechname[sasl->len] = '\0';
    spice_info("Got client mechname '%s' check against '%s'",
               sasl->mechname, sasl->mechlist);

    if (strncmp(sasl->mechlist, sasl->mechname, sasl->len) == 0) {
        if (sasl->mechlist[sasl->len] != '\0' &&
            sasl->mechlist[sasl->len] != ',') {
            spice_info("One %d", sasl->mechlist[sasl->len]);
            return FALSE;
        }
    } else {
        char *offset = strstr(sasl->mechlist, sasl->mechname);
        spice_info("Two %p", offset);
        if (!offset) {
            return FALSE;
        }
        spice_info("Two '%s'", offset);
        if (offset[-1] != ',' ||
            (offset[sasl->len] != '\0'&&
             offset[sasl->len] != ',')) {
            return FALSE;
        }
    }

    free(sasl->mechlist);
    sasl->mechlist = spice_strdup(sasl->mechname);

    spice_info("Validated mechname '%s'", sasl->mechname);

    reds_stream_async_read(stream, (uint8_t *)&sasl->len, sizeof(uint32_t),
                           read_cb, opaque);

    return TRUE;
}

bool reds_sasl_handle_auth_mechlen(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    RedsSASL *sasl = &stream->priv->sasl;

    if (sasl->len < 1 || sasl->len > 100) {
        spice_warning("Got bad client mechname len %d", sasl->len);
        return FALSE;
    }

    sasl->mechname = spice_malloc(sasl->len + 1);

    spice_info("Wait for client mechname");
    reds_stream_async_read(stream, (uint8_t *)sasl->mechname, sasl->len,
                           read_cb, opaque);

    return TRUE;
}

bool reds_sasl_start_auth(RedsStream *stream, AsyncReadDone read_cb, void *opaque)
{
    const char *mechlist = NULL;
    sasl_security_properties_t secprops;
    int err;
    char *localAddr, *remoteAddr;
    int mechlistlen;
    RedsSASL *sasl = &stream->priv->sasl;

    if (!(localAddr = reds_stream_get_local_address(stream))) {
        goto error;
    }

    if (!(remoteAddr = reds_stream_get_remote_address(stream))) {
        free(localAddr);
        goto error;
    }

    err = sasl_server_new("spice",
                          NULL, /* FQDN - just delegates to gethostname */
                          NULL, /* User realm */
                          localAddr,
                          remoteAddr,
                          NULL, /* Callbacks, not needed */
                          SASL_SUCCESS_DATA,
                          &sasl->conn);
    free(localAddr);
    free(remoteAddr);
    localAddr = remoteAddr = NULL;

    if (err != SASL_OK) {
        spice_warning("sasl context setup failed %d (%s)",
                    err, sasl_errstring(err, NULL, NULL));
        sasl->conn = NULL;
        goto error;
    }

    /* Inform SASL that we've got an external SSF layer from TLS */
    if (stream->priv->ssl) {
        sasl_ssf_t ssf;

        ssf = SSL_get_cipher_bits(stream->priv->ssl, NULL);
        err = sasl_setprop(sasl->conn, SASL_SSF_EXTERNAL, &ssf);
        if (err != SASL_OK) {
            spice_warning("cannot set SASL external SSF %d (%s)",
                        err, sasl_errstring(err, NULL, NULL));
            goto error_dispose;
        }
    } else {
        sasl->wantSSF = 1;
    }

    memset(&secprops, 0, sizeof secprops);
    /* Inform SASL that we've got an external SSF layer from TLS */
    if (stream->priv->ssl) {
        /* If we've got TLS (or UNIX domain sock), we don't care about SSF */
        secprops.min_ssf = 0;
        secprops.max_ssf = 0;
        secprops.maxbufsize = 8192;
        secprops.security_flags = 0;
    } else {
        /* Plain TCP, better get an SSF layer */
        secprops.min_ssf = 56; /* Good enough to require kerberos */
        secprops.max_ssf = 100000; /* Arbitrary big number */
        secprops.maxbufsize = 8192;
        /* Forbid any anonymous or trivially crackable auth */
        secprops.security_flags =
            SASL_SEC_NOANONYMOUS | SASL_SEC_NOPLAINTEXT;
    }

    err = sasl_setprop(sasl->conn, SASL_SEC_PROPS, &secprops);
    if (err != SASL_OK) {
        spice_warning("cannot set SASL security props %d (%s)",
                      err, sasl_errstring(err, NULL, NULL));
        goto error_dispose;
    }

    err = sasl_listmech(sasl->conn,
                        NULL, /* Don't need to set user */
                        "", /* Prefix */
                        ",", /* Separator */
                        "", /* Suffix */
                        &mechlist,
                        NULL,
                        NULL);
    if (err != SASL_OK || mechlist == NULL) {
        spice_warning("cannot list SASL mechanisms %d (%s)",
                      err, sasl_errdetail(sasl->conn));
        goto error_dispose;
    }

    spice_info("Available mechanisms for client: '%s'", mechlist);

    sasl->mechlist = spice_strdup(mechlist);

    mechlistlen = strlen(mechlist);
    if (!reds_stream_write_all(stream, &mechlistlen, sizeof(uint32_t))
        || !reds_stream_write_all(stream, sasl->mechlist, mechlistlen)) {
        spice_warning("SASL mechanisms write error");
        goto error;
    }

    spice_info("Wait for client mechname length");
    reds_stream_async_read(stream, (uint8_t *)&sasl->len, sizeof(uint32_t),
                           read_cb, opaque);

    return TRUE;

error_dispose:
    sasl_dispose(&sasl->conn);
    sasl->conn = NULL;
error:
    return FALSE;
}
#endif
