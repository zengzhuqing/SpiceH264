/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

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

#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "common/marshaller.h"
#include "common/generated_server_marshallers.h"

#include "spice.h"
#include "red_common.h"
#include "main_channel.h"
#include "reds.h"
#include "red_dispatcher.h"
#include "snd_worker.h"
#include "common/snd_codec.h"
#include "demarshallers.h"

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

#define SND_RECEIVE_BUF_SIZE     (16 * 1024 * 2)
#define RECORD_SAMPLES_SIZE (SND_RECEIVE_BUF_SIZE >> 2)

enum PlaybackCommand {
    SND_PLAYBACK_MIGRATE,
    SND_PLAYBACK_MODE,
    SND_PLAYBACK_CTRL,
    SND_PLAYBACK_PCM,
    SND_PLAYBACK_VOLUME,
    SND_PLAYBACK_LATENCY,
};

enum RecordCommand {
    SND_RECORD_MIGRATE,
    SND_RECORD_CTRL,
    SND_RECORD_VOLUME,
};

#define SND_PLAYBACK_MIGRATE_MASK (1 << SND_PLAYBACK_MIGRATE)
#define SND_PLAYBACK_MODE_MASK (1 << SND_PLAYBACK_MODE)
#define SND_PLAYBACK_CTRL_MASK (1 << SND_PLAYBACK_CTRL)
#define SND_PLAYBACK_PCM_MASK (1 << SND_PLAYBACK_PCM)
#define SND_PLAYBACK_VOLUME_MASK (1 << SND_PLAYBACK_VOLUME)
#define SND_PLAYBACK_LATENCY_MASK ( 1 << SND_PLAYBACK_LATENCY)

#define SND_RECORD_MIGRATE_MASK (1 << SND_RECORD_MIGRATE)
#define SND_RECORD_CTRL_MASK (1 << SND_RECORD_CTRL)
#define SND_RECORD_VOLUME_MASK (1 << SND_RECORD_VOLUME)

typedef struct SndChannel SndChannel;
typedef void (*snd_channel_send_messages_proc)(void *in_channel);
typedef int (*snd_channel_handle_message_proc)(SndChannel *channel, size_t size, uint32_t type, void *message);
typedef void (*snd_channel_on_message_done_proc)(SndChannel *channel);
typedef void (*snd_channel_cleanup_channel_proc)(SndChannel *channel);

typedef struct SndWorker SndWorker;

struct SndChannel {
    RedsStream *stream;
    SndWorker *worker;
    spice_parse_channel_func_t parser;
    int refs;

    RedChannelClient *channel_client;

    int active;
    int client_active;
    int blocked;

    uint32_t command;
    uint32_t ack_generation;
    uint32_t client_ack_generation;
    uint32_t out_messages;
    uint32_t ack_messages;

    struct {
        uint64_t serial;
        SpiceMarshaller *marshaller;
        uint32_t size;
        uint32_t pos;
    } send_data;

    struct {
        uint8_t buf[SND_RECEIVE_BUF_SIZE];
        uint8_t *message_start;
        uint8_t *now;
        uint8_t *end;
    } receive_data;

    snd_channel_send_messages_proc send_messages;
    snd_channel_handle_message_proc handle_message;
    snd_channel_on_message_done_proc on_message_done;
    snd_channel_cleanup_channel_proc cleanup;
};

typedef struct PlaybackChannel PlaybackChannel;

typedef struct AudioFrame AudioFrame;
struct AudioFrame {
    uint32_t time;
    uint32_t samples[SND_CODEC_MAX_FRAME_SIZE];
    PlaybackChannel *channel;
    AudioFrame *next;
};

struct PlaybackChannel {
    SndChannel base;
    AudioFrame frames[3];
    AudioFrame *free_frames;
    AudioFrame *in_progress;
    AudioFrame *pending_frame;
    uint32_t mode;
    uint32_t latency;
    SndCodec codec;
    uint8_t  encode_buf[SND_CODEC_MAX_COMPRESSED_BYTES];
};

struct SndWorker {
    RedChannel *base_channel;
    SndChannel *connection;
    SndWorker *next;
    int active;
};

typedef struct SpiceVolumeState {
    uint8_t volume_nchannels;
    uint16_t *volume;
    int mute;
} SpiceVolumeState;

struct SpicePlaybackState {
    struct SndWorker worker;
    SpicePlaybackInstance *sin;
    SpiceVolumeState volume;
    uint32_t frequency;
};

struct SpiceRecordState {
    struct SndWorker worker;
    SpiceRecordInstance *sin;
    SpiceVolumeState volume;
    uint32_t frequency;
};

typedef struct RecordChannel {
    SndChannel base;
    uint32_t samples[RECORD_SAMPLES_SIZE];
    uint32_t write_pos;
    uint32_t read_pos;
    uint32_t mode;
    uint32_t mode_time;
    uint32_t start_time;
    SndCodec codec;
    uint8_t  decode_buf[SND_CODEC_MAX_FRAME_BYTES];
} RecordChannel;

static SndWorker *workers;
static uint32_t playback_compression = TRUE;

static void snd_receive(void* data);

static SndChannel *snd_channel_get(SndChannel *channel)
{
    channel->refs++;
    return channel;
}

static SndChannel *snd_channel_put(SndChannel *channel)
{
    if (!--channel->refs) {
        spice_printerr("SndChannel=%p freed", channel);
        free(channel);
        return NULL;
    }
    return channel;
}

static void snd_disconnect_channel(SndChannel *channel)
{
    SndWorker *worker;

    if (!channel || !channel->stream) {
        spice_debug("not connected");
        return;
    }
    spice_debug("SndChannel=%p rcc=%p type=%d",
                 channel, channel->channel_client, channel->channel_client->channel->type);
    worker = channel->worker;
    channel->cleanup(channel);
    red_channel_client_disconnect(worker->connection->channel_client);
    worker->connection->channel_client = NULL;
    core->watch_remove(channel->stream->watch);
    channel->stream->watch = NULL;
    reds_stream_free(channel->stream);
    channel->stream = NULL;
    spice_marshaller_destroy(channel->send_data.marshaller);
    snd_channel_put(channel);
    worker->connection = NULL;
}

static void snd_playback_free_frame(PlaybackChannel *playback_channel, AudioFrame *frame)
{
    frame->channel = playback_channel;
    frame->next = playback_channel->free_frames;
    playback_channel->free_frames = frame;
}

static void snd_playback_on_message_done(SndChannel *channel)
{
    PlaybackChannel *playback_channel = (PlaybackChannel *)channel;
    if (playback_channel->in_progress) {
        snd_playback_free_frame(playback_channel, playback_channel->in_progress);
        playback_channel->in_progress = NULL;
        if (playback_channel->pending_frame) {
            channel->command |= SND_PLAYBACK_PCM_MASK;
        }
    }
}

static void snd_record_on_message_done(SndChannel *channel)
{
}

static int snd_send_data(SndChannel *channel)
{
    uint32_t n;

    if (!channel) {
        return FALSE;
    }

    if (!(n = channel->send_data.size - channel->send_data.pos)) {
        return TRUE;
    }

    for (;;) {
        struct iovec vec[IOV_MAX];
        int vec_size;

        if (!n) {
            channel->on_message_done(channel);

            if (channel->blocked) {
                channel->blocked = FALSE;
                core->watch_update_mask(channel->stream->watch, SPICE_WATCH_EVENT_READ);
            }
            break;
        }

        vec_size = spice_marshaller_fill_iovec(channel->send_data.marshaller,
                                               vec, IOV_MAX, channel->send_data.pos);
        n = reds_stream_writev(channel->stream, vec, vec_size);
        if (n == -1) {
            switch (errno) {
            case EAGAIN:
                channel->blocked = TRUE;
                core->watch_update_mask(channel->stream->watch, SPICE_WATCH_EVENT_READ |
                                        SPICE_WATCH_EVENT_WRITE);
                return FALSE;
            case EINTR:
                break;
            case EPIPE:
                snd_disconnect_channel(channel);
                return FALSE;
            default:
                spice_printerr("%s", strerror(errno));
                snd_disconnect_channel(channel);
                return FALSE;
            }
        } else {
            channel->send_data.pos += n;
        }
        n = channel->send_data.size - channel->send_data.pos;
    }
    return TRUE;
}

static int snd_record_handle_write(RecordChannel *record_channel, size_t size, void *message)
{
    SpiceMsgcRecordPacket *packet;
    uint32_t write_pos;
    uint32_t* data;
    uint32_t len;
    uint32_t now;

    if (!record_channel) {
        return FALSE;
    }

    packet = (SpiceMsgcRecordPacket *)message;

    if (record_channel->mode == SPICE_AUDIO_DATA_MODE_RAW) {
        data = (uint32_t *)packet->data;
        size = packet->data_size >> 2;
        size = MIN(size, RECORD_SAMPLES_SIZE);
     } else {
        int decode_size;
        decode_size = sizeof(record_channel->decode_buf);
        if (snd_codec_decode(record_channel->codec, packet->data, packet->data_size,
                    record_channel->decode_buf, &decode_size) != SND_CODEC_OK)
            return FALSE;
        data = (uint32_t *) record_channel->decode_buf;
        size = decode_size >> 2;
    }

    write_pos = record_channel->write_pos % RECORD_SAMPLES_SIZE;
    record_channel->write_pos += size;
    len = RECORD_SAMPLES_SIZE - write_pos;
    now = MIN(len, size);
    size -= now;
    memcpy(record_channel->samples + write_pos, data, now << 2);

    if (size) {
        memcpy(record_channel->samples, data + now, size << 2);
    }

    if (record_channel->write_pos - record_channel->read_pos > RECORD_SAMPLES_SIZE) {
        record_channel->read_pos = record_channel->write_pos - RECORD_SAMPLES_SIZE;
    }
    return TRUE;
}

static int snd_playback_handle_message(SndChannel *channel, size_t size, uint32_t type, void *message)
{
    if (!channel) {
        return FALSE;
    }

    switch (type) {
    case SPICE_MSGC_DISCONNECTING:
        break;
    default:
        spice_printerr("invalid message type %u", type);
        return FALSE;
    }
    return TRUE;
}

static int snd_record_handle_message(SndChannel *channel, size_t size, uint32_t type, void *message)
{
    RecordChannel *record_channel = (RecordChannel *)channel;

    if (!channel) {
        return FALSE;
    }
    switch (type) {
    case SPICE_MSGC_RECORD_DATA:
        return snd_record_handle_write((RecordChannel *)channel, size, message);
    case SPICE_MSGC_RECORD_MODE: {
        SpiceMsgcRecordMode *mode = (SpiceMsgcRecordMode *)message;
        SpiceRecordState *st = SPICE_CONTAINEROF(channel->worker, SpiceRecordState, worker);
        record_channel->mode_time = mode->time;
        if (mode->mode != SPICE_AUDIO_DATA_MODE_RAW) {
            if (snd_codec_is_capable(mode->mode, st->frequency)) {
                if (snd_codec_create(&record_channel->codec, mode->mode, st->frequency, SND_CODEC_DECODE) == SND_CODEC_OK) {
                    record_channel->mode = mode->mode;
                } else {
                    spice_printerr("create decoder failed");
                    return FALSE;
                }
            }
            else {
                spice_printerr("unsupported mode %d", record_channel->mode);
                return FALSE;
            }
        }
        else
            record_channel->mode = mode->mode;
        break;
    }

    case SPICE_MSGC_RECORD_START_MARK: {
        SpiceMsgcRecordStartMark *mark = (SpiceMsgcRecordStartMark *)message;
        record_channel->start_time = mark->time;
        break;
    }
    case SPICE_MSGC_DISCONNECTING:
        break;
    default:
        spice_printerr("invalid message type %u", type);
        return FALSE;
    }
    return TRUE;
}

static void snd_receive(void* data)
{
    SndChannel *channel = (SndChannel*)data;
    SpiceDataHeaderOpaque *header;

    if (!channel) {
        return;
    }

    header = &channel->channel_client->incoming.header;

    for (;;) {
        ssize_t n;
        n = channel->receive_data.end - channel->receive_data.now;
        spice_warn_if(n <= 0);
        n = reds_stream_read(channel->stream, channel->receive_data.now, n);
        if (n <= 0) {
            if (n == 0) {
                snd_disconnect_channel(channel);
                return;
            }
            spice_assert(n == -1);
            switch (errno) {
            case EAGAIN:
                return;
            case EINTR:
                break;
            case EPIPE:
                snd_disconnect_channel(channel);
                return;
            default:
                spice_printerr("%s", strerror(errno));
                snd_disconnect_channel(channel);
                return;
            }
        } else {
            channel->receive_data.now += n;
            for (;;) {
                uint8_t *msg_start = channel->receive_data.message_start;
                uint8_t *data = msg_start + header->header_size;
                size_t parsed_size;
                uint8_t *parsed;
                message_destructor_t parsed_free;

                header->data = msg_start;
                n = channel->receive_data.now - msg_start;

                if (n < header->header_size ||
                    n < header->header_size + header->get_msg_size(header)) {
                    break;
                }
                parsed = channel->parser((void *)data, data + header->get_msg_size(header),
                                         header->get_msg_type(header),
                                         SPICE_VERSION_MINOR, &parsed_size, &parsed_free);
                if (parsed == NULL) {
                    spice_printerr("failed to parse message type %d", header->get_msg_type(header));
                    snd_disconnect_channel(channel);
                    return;
                }
                if (!channel->handle_message(channel, parsed_size,
                                             header->get_msg_type(header), parsed)) {
                    free(parsed);
                    snd_disconnect_channel(channel);
                    return;
                }
                parsed_free(parsed);
                channel->receive_data.message_start = msg_start + header->header_size +
                                                     header->get_msg_size(header);
            }
            if (channel->receive_data.now == channel->receive_data.message_start) {
                channel->receive_data.now = channel->receive_data.buf;
                channel->receive_data.message_start = channel->receive_data.buf;
            } else if (channel->receive_data.now == channel->receive_data.end) {
                memcpy(channel->receive_data.buf, channel->receive_data.message_start, n);
                channel->receive_data.now = channel->receive_data.buf + n;
                channel->receive_data.message_start = channel->receive_data.buf;
            }
        }
    }
}

static void snd_event(int fd, int event, void *data)
{
    SndChannel *channel = data;

    if (event & SPICE_WATCH_EVENT_READ) {
        snd_receive(channel);
    }
    if (event & SPICE_WATCH_EVENT_WRITE) {
        channel->send_messages(channel);
    }
}

static inline int snd_reset_send_data(SndChannel *channel, uint16_t verb)
{
    SpiceDataHeaderOpaque *header;

    if (!channel) {
        return FALSE;
    }

    header = &channel->channel_client->send_data.header;
    spice_marshaller_reset(channel->send_data.marshaller);
    header->data = spice_marshaller_reserve_space(channel->send_data.marshaller,
                                                  header->header_size);
    spice_marshaller_set_base(channel->send_data.marshaller,
                              header->header_size);
    channel->send_data.pos = 0;
    header->set_msg_size(header, 0);
    header->set_msg_type(header, verb);
    channel->send_data.serial++;
    if (!channel->channel_client->is_mini_header) {
        header->set_msg_serial(header, channel->send_data.serial);
        header->set_msg_sub_list(header, 0);
    }

    return TRUE;
}

static int snd_begin_send_message(SndChannel *channel)
{
    SpiceDataHeaderOpaque *header = &channel->channel_client->send_data.header;

    spice_marshaller_flush(channel->send_data.marshaller);
    channel->send_data.size = spice_marshaller_get_total_size(channel->send_data.marshaller);
    header->set_msg_size(header, channel->send_data.size - header->header_size);
    return snd_send_data(channel);
}

static int snd_channel_send_migrate(SndChannel *channel)
{
    SpiceMsgMigrate migrate;

    if (!snd_reset_send_data(channel, SPICE_MSG_MIGRATE)) {
        return FALSE;
    }
    spice_debug(NULL);
    migrate.flags = 0;
    spice_marshall_msg_migrate(channel->send_data.marshaller, &migrate);

    return snd_begin_send_message(channel);
}

static int snd_playback_send_migrate(PlaybackChannel *channel)
{
    return snd_channel_send_migrate(&channel->base);
}

static int snd_send_volume(SndChannel *channel, SpiceVolumeState *st, int msg)
{
    SpiceMsgAudioVolume *vol;
    uint8_t c;

    vol = alloca(sizeof (SpiceMsgAudioVolume) +
                 st->volume_nchannels * sizeof (uint16_t));
    if (!snd_reset_send_data(channel, msg)) {
        return FALSE;
    }
    vol->nchannels = st->volume_nchannels;
    for (c = 0; c < st->volume_nchannels; ++c) {
        vol->volume[c] = st->volume[c];
    }
    spice_marshall_SpiceMsgAudioVolume(channel->send_data.marshaller, vol);

    return snd_begin_send_message(channel);
}

static int snd_playback_send_volume(PlaybackChannel *playback_channel)
{
    SndChannel *channel = &playback_channel->base;
    SpicePlaybackState *st = SPICE_CONTAINEROF(channel->worker, SpicePlaybackState, worker);

    if (!red_channel_client_test_remote_cap(channel->channel_client,
                                            SPICE_PLAYBACK_CAP_VOLUME)) {
        return TRUE;
    }

    return snd_send_volume(channel, &st->volume, SPICE_MSG_PLAYBACK_VOLUME);
}

static int snd_send_mute(SndChannel *channel, SpiceVolumeState *st, int msg)
{
    SpiceMsgAudioMute mute;

    if (!snd_reset_send_data(channel, msg)) {
        return FALSE;
    }
    mute.mute = st->mute;
    spice_marshall_SpiceMsgAudioMute(channel->send_data.marshaller, &mute);

    return snd_begin_send_message(channel);
}

static int snd_playback_send_mute(PlaybackChannel *playback_channel)
{
    SndChannel *channel = &playback_channel->base;
    SpicePlaybackState *st = SPICE_CONTAINEROF(channel->worker, SpicePlaybackState, worker);

    if (!red_channel_client_test_remote_cap(channel->channel_client,
                                            SPICE_PLAYBACK_CAP_VOLUME)) {
        return TRUE;
    }

    return snd_send_mute(channel, &st->volume, SPICE_MSG_PLAYBACK_MUTE);
}

static int snd_playback_send_latency(PlaybackChannel *playback_channel)
{
    SndChannel *channel = &playback_channel->base;
    SpiceMsgPlaybackLatency latency_msg;

    spice_debug("latency %u", playback_channel->latency);
    if (!snd_reset_send_data(channel, SPICE_MSG_PLAYBACK_LATENCY)) {
        return FALSE;
    }
    latency_msg.latency_ms = playback_channel->latency;
    spice_marshall_msg_playback_latency(channel->send_data.marshaller, &latency_msg);

    return snd_begin_send_message(channel);
}
static int snd_playback_send_start(PlaybackChannel *playback_channel)
{
    SndChannel *channel = (SndChannel *)playback_channel;
    SpicePlaybackState *st = SPICE_CONTAINEROF(channel->worker, SpicePlaybackState, worker);
    SpiceMsgPlaybackStart start;

    if (!snd_reset_send_data(channel, SPICE_MSG_PLAYBACK_START)) {
        return FALSE;
    }

    start.channels = SPICE_INTERFACE_PLAYBACK_CHAN;
    start.frequency = st->frequency;
    spice_assert(SPICE_INTERFACE_PLAYBACK_FMT == SPICE_INTERFACE_AUDIO_FMT_S16);
    start.format = SPICE_AUDIO_FMT_S16;
    start.time = reds_get_mm_time();
    spice_marshall_msg_playback_start(channel->send_data.marshaller, &start);

    return snd_begin_send_message(channel);
}

static int snd_playback_send_stop(PlaybackChannel *playback_channel)
{
    SndChannel *channel = (SndChannel *)playback_channel;

    if (!snd_reset_send_data(channel, SPICE_MSG_PLAYBACK_STOP)) {
        return FALSE;
    }

    return snd_begin_send_message(channel);
}

static int snd_playback_send_ctl(PlaybackChannel *playback_channel)
{
    SndChannel *channel = (SndChannel *)playback_channel;

    if ((channel->client_active = channel->active)) {
        return snd_playback_send_start(playback_channel);
    } else {
        return snd_playback_send_stop(playback_channel);
    }
}

static int snd_record_send_start(RecordChannel *record_channel)
{
    SndChannel *channel = (SndChannel *)record_channel;
    SpiceRecordState *st = SPICE_CONTAINEROF(channel->worker, SpiceRecordState, worker);
    SpiceMsgRecordStart start;

    if (!snd_reset_send_data(channel, SPICE_MSG_RECORD_START)) {
        return FALSE;
    }

    start.channels = SPICE_INTERFACE_RECORD_CHAN;
    start.frequency = st->frequency;
    spice_assert(SPICE_INTERFACE_RECORD_FMT == SPICE_INTERFACE_AUDIO_FMT_S16);
    start.format = SPICE_AUDIO_FMT_S16;
    spice_marshall_msg_record_start(channel->send_data.marshaller, &start);

    return snd_begin_send_message(channel);
}

static int snd_record_send_stop(RecordChannel *record_channel)
{
    SndChannel *channel = (SndChannel *)record_channel;

    if (!snd_reset_send_data(channel, SPICE_MSG_RECORD_STOP)) {
        return FALSE;
    }

    return snd_begin_send_message(channel);
}

static int snd_record_send_ctl(RecordChannel *record_channel)
{
    SndChannel *channel = (SndChannel *)record_channel;

    if ((channel->client_active = channel->active)) {
        return snd_record_send_start(record_channel);
    } else {
        return snd_record_send_stop(record_channel);
    }
}

static int snd_record_send_volume(RecordChannel *record_channel)
{
    SndChannel *channel = &record_channel->base;
    SpiceRecordState *st = SPICE_CONTAINEROF(channel->worker, SpiceRecordState, worker);

    if (!red_channel_client_test_remote_cap(channel->channel_client,
                                            SPICE_RECORD_CAP_VOLUME)) {
        return TRUE;
    }

    return snd_send_volume(channel, &st->volume, SPICE_MSG_RECORD_VOLUME);
}

static int snd_record_send_mute(RecordChannel *record_channel)
{
    SndChannel *channel = &record_channel->base;
    SpiceRecordState *st = SPICE_CONTAINEROF(channel->worker, SpiceRecordState, worker);

    if (!red_channel_client_test_remote_cap(channel->channel_client,
                                            SPICE_RECORD_CAP_VOLUME)) {
        return TRUE;
    }

    return snd_send_mute(channel, &st->volume, SPICE_MSG_RECORD_MUTE);
}

static int snd_record_send_migrate(RecordChannel *record_channel)
{
    /* No need for migration data: if recording has started before migration,
     * the client receives RECORD_STOP from the src before the migration completion
     * notification (when the vm is stopped).
     * Afterwards, when the vm starts on the dest, the client receives RECORD_START. */
    return snd_channel_send_migrate(&record_channel->base);
}

static int snd_playback_send_write(PlaybackChannel *playback_channel)
{
    SndChannel *channel = (SndChannel *)playback_channel;
    AudioFrame *frame;
    SpiceMsgPlaybackPacket msg;

    if (!snd_reset_send_data(channel, SPICE_MSG_PLAYBACK_DATA)) {
        return FALSE;
    }

    frame = playback_channel->in_progress;
    msg.time = frame->time;

    spice_marshall_msg_playback_data(channel->send_data.marshaller, &msg);

    if (playback_channel->mode == SPICE_AUDIO_DATA_MODE_RAW) {
        spice_marshaller_add_ref(channel->send_data.marshaller,
                                 (uint8_t *)frame->samples,
                                 snd_codec_frame_size(playback_channel->codec) * sizeof(frame->samples[0]));
    }
    else {
        int n = sizeof(playback_channel->encode_buf);
        if (snd_codec_encode(playback_channel->codec, (uint8_t *) frame->samples,
                                    snd_codec_frame_size(playback_channel->codec) * sizeof(frame->samples[0]),
                                    playback_channel->encode_buf, &n) != SND_CODEC_OK) {
            spice_printerr("encode failed");
            snd_disconnect_channel(channel);
            return FALSE;
        }
        spice_marshaller_add_ref(channel->send_data.marshaller, playback_channel->encode_buf, n);
    }

    return snd_begin_send_message(channel);
}

static int playback_send_mode(PlaybackChannel *playback_channel)
{
    SndChannel *channel = (SndChannel *)playback_channel;
    SpiceMsgPlaybackMode mode;

    if (!snd_reset_send_data(channel, SPICE_MSG_PLAYBACK_MODE)) {
        return FALSE;
    }
    mode.time = reds_get_mm_time();
    mode.mode = playback_channel->mode;
    spice_marshall_msg_playback_mode(channel->send_data.marshaller, &mode);

    return snd_begin_send_message(channel);
}

static void snd_playback_send(void* data)
{
    PlaybackChannel *playback_channel = (PlaybackChannel*)data;
    SndChannel *channel = (SndChannel*)playback_channel;

    if (!playback_channel || !snd_send_data(data)) {
        return;
    }

    while (channel->command) {
        if (channel->command & SND_PLAYBACK_MODE_MASK) {
            if (!playback_send_mode(playback_channel)) {
                return;
            }
            channel->command &= ~SND_PLAYBACK_MODE_MASK;
        }
        if (channel->command & SND_PLAYBACK_PCM_MASK) {
            spice_assert(!playback_channel->in_progress && playback_channel->pending_frame);
            playback_channel->in_progress = playback_channel->pending_frame;
            playback_channel->pending_frame = NULL;
            channel->command &= ~SND_PLAYBACK_PCM_MASK;
            if (!snd_playback_send_write(playback_channel)) {
                spice_printerr("snd_send_playback_write failed");
                return;
            }
        }
        if (channel->command & SND_PLAYBACK_CTRL_MASK) {
            if (!snd_playback_send_ctl(playback_channel)) {
                return;
            }
            channel->command &= ~SND_PLAYBACK_CTRL_MASK;
        }
        if (channel->command & SND_PLAYBACK_VOLUME_MASK) {
            if (!snd_playback_send_volume(playback_channel) ||
                !snd_playback_send_mute(playback_channel)) {
                return;
            }
            channel->command &= ~SND_PLAYBACK_VOLUME_MASK;
        }
        if (channel->command & SND_PLAYBACK_MIGRATE_MASK) {
            if (!snd_playback_send_migrate(playback_channel)) {
                return;
            }
            channel->command &= ~SND_PLAYBACK_MIGRATE_MASK;
        }
        if (channel->command & SND_PLAYBACK_LATENCY_MASK) {
            if (!snd_playback_send_latency(playback_channel)) {
                return;
            }
            channel->command &= ~SND_PLAYBACK_LATENCY_MASK;
        }
    }
}

static void snd_record_send(void* data)
{
    RecordChannel *record_channel = (RecordChannel*)data;
    SndChannel *channel = (SndChannel*)record_channel;

    if (!record_channel || !snd_send_data(data)) {
        return;
    }

    while (channel->command) {
        if (channel->command & SND_RECORD_CTRL_MASK) {
            if (!snd_record_send_ctl(record_channel)) {
                return;
            }
            channel->command &= ~SND_RECORD_CTRL_MASK;
        }
        if (channel->command & SND_RECORD_VOLUME_MASK) {
            if (!snd_record_send_volume(record_channel) ||
                !snd_record_send_mute(record_channel)) {
                return;
            }
            channel->command &= ~SND_RECORD_VOLUME_MASK;
        }
        if (channel->command & SND_RECORD_MIGRATE_MASK) {
            if (!snd_record_send_migrate(record_channel)) {
                return;
            }
            channel->command &= ~SND_RECORD_MIGRATE_MASK;
        }
    }
}

static SndChannel *__new_channel(SndWorker *worker, int size, uint32_t channel_id,
                                 RedClient *client,
                                 RedsStream *stream,
                                 int migrate,
                                 snd_channel_send_messages_proc send_messages,
                                 snd_channel_handle_message_proc handle_message,
                                 snd_channel_on_message_done_proc on_message_done,
                                 snd_channel_cleanup_channel_proc cleanup,
                                 uint32_t *common_caps, int num_common_caps,
                                 uint32_t *caps, int num_caps)
{
    SndChannel *channel;
    int delay_val;
    int flags;
#ifdef SO_PRIORITY
    int priority;
#endif
    int tos;
    MainChannelClient *mcc = red_client_get_main(client);

    spice_assert(stream);
    if ((flags = fcntl(stream->socket, F_GETFL)) == -1) {
        spice_printerr("accept failed, %s", strerror(errno));
        goto error1;
    }

#ifdef SO_PRIORITY
    priority = 6;
    if (setsockopt(stream->socket, SOL_SOCKET, SO_PRIORITY, (void*)&priority,
                   sizeof(priority)) == -1) {
        if (errno != ENOTSUP) {
            spice_printerr("setsockopt failed, %s", strerror(errno));
        }
    }
#endif

    tos = IPTOS_LOWDELAY;
    if (setsockopt(stream->socket, IPPROTO_IP, IP_TOS, (void*)&tos, sizeof(tos)) == -1) {
        if (errno != ENOTSUP) {
            spice_printerr("setsockopt failed, %s", strerror(errno));
        }
    }

    delay_val = main_channel_client_is_low_bandwidth(mcc) ? 0 : 1;
    if (setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY, &delay_val, sizeof(delay_val)) == -1) {
        if (errno != ENOTSUP) {
            spice_printerr("setsockopt failed, %s", strerror(errno));
        }
    }

    if (fcntl(stream->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        spice_printerr("accept failed, %s", strerror(errno));
        goto error1;
    }

    spice_assert(size >= sizeof(*channel));
    channel = spice_malloc0(size);
    channel->refs = 1;
    channel->parser = spice_get_client_channel_parser(channel_id, NULL);
    channel->stream = stream;
    channel->worker = worker;
    channel->receive_data.message_start = channel->receive_data.buf;
    channel->receive_data.now = channel->receive_data.buf;
    channel->receive_data.end = channel->receive_data.buf + sizeof(channel->receive_data.buf);
    channel->send_data.marshaller = spice_marshaller_new();

    stream->watch = core->watch_add(stream->socket, SPICE_WATCH_EVENT_READ,
                                  snd_event, channel);
    if (stream->watch == NULL) {
        spice_printerr("watch_add failed, %s", strerror(errno));
        goto error2;
    }

    channel->send_messages = send_messages;
    channel->handle_message = handle_message;
    channel->on_message_done = on_message_done;
    channel->cleanup = cleanup;

    channel->channel_client = red_channel_client_create_dummy(sizeof(RedChannelClient),
                                                              worker->base_channel,
                                                              client,
                                                              num_common_caps, common_caps,
                                                              num_caps, caps);
    if (!channel->channel_client) {
        goto error2;
    }
    return channel;

error2:
    free(channel);

error1:
    reds_stream_free(stream);
    return NULL;
}

static void snd_disconnect_channel_client(RedChannelClient *rcc)
{
    SndWorker *worker;

    spice_assert(rcc->channel);
    spice_assert(rcc->channel->data);
    worker = (SndWorker *)rcc->channel->data;

    spice_debug("channel-type=%d", rcc->channel->type);
    if (worker->connection) {
        spice_assert(worker->connection->channel_client == rcc);
        snd_disconnect_channel(worker->connection);
    }
}

static void snd_set_command(SndChannel *channel, uint32_t command)
{
    if (!channel) {
        return;
    }
    channel->command |= command;
}

SPICE_GNUC_VISIBLE void spice_server_playback_set_volume(SpicePlaybackInstance *sin,
                                                  uint8_t nchannels,
                                                  uint16_t *volume)
{
    SpiceVolumeState *st = &sin->st->volume;
    SndChannel *channel = sin->st->worker.connection;
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    st->volume_nchannels = nchannels;
    free(st->volume);
    st->volume = spice_memdup(volume, sizeof(uint16_t) * nchannels);

    if (!channel || nchannels == 0)
        return;

    snd_playback_send_volume(playback_channel);
}

SPICE_GNUC_VISIBLE void spice_server_playback_set_mute(SpicePlaybackInstance *sin, uint8_t mute)
{
    SpiceVolumeState *st = &sin->st->volume;
    SndChannel *channel = sin->st->worker.connection;
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    st->mute = mute;

    if (!channel)
        return;

    snd_playback_send_mute(playback_channel);
}

SPICE_GNUC_VISIBLE void spice_server_playback_start(SpicePlaybackInstance *sin)
{
    SndChannel *channel = sin->st->worker.connection;
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    sin->st->worker.active = 1;
    if (!channel)
        return;
    spice_assert(!playback_channel->base.active);
    reds_disable_mm_time();
    playback_channel->base.active = TRUE;
    if (!playback_channel->base.client_active) {
        snd_set_command(&playback_channel->base, SND_PLAYBACK_CTRL_MASK);
        snd_playback_send(&playback_channel->base);
    } else {
        playback_channel->base.command &= ~SND_PLAYBACK_CTRL_MASK;
    }
}

SPICE_GNUC_VISIBLE void spice_server_playback_stop(SpicePlaybackInstance *sin)
{
    SndChannel *channel = sin->st->worker.connection;
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    sin->st->worker.active = 0;
    if (!channel)
        return;
    spice_assert(playback_channel->base.active);
    reds_enable_mm_time();
    playback_channel->base.active = FALSE;
    if (playback_channel->base.client_active) {
        snd_set_command(&playback_channel->base, SND_PLAYBACK_CTRL_MASK);
        snd_playback_send(&playback_channel->base);
    } else {
        playback_channel->base.command &= ~SND_PLAYBACK_CTRL_MASK;
        playback_channel->base.command &= ~SND_PLAYBACK_PCM_MASK;

        if (playback_channel->pending_frame) {
            spice_assert(!playback_channel->in_progress);
            snd_playback_free_frame(playback_channel,
                                    playback_channel->pending_frame);
            playback_channel->pending_frame = NULL;
        }
    }
}

SPICE_GNUC_VISIBLE void spice_server_playback_get_buffer(SpicePlaybackInstance *sin,
                                                         uint32_t **frame, uint32_t *num_samples)
{
    SndChannel *channel = sin->st->worker.connection;
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    if (!channel || !playback_channel->free_frames) {
        *frame = NULL;
        *num_samples = 0;
        return;
    }
    spice_assert(playback_channel->base.active);
    snd_channel_get(channel);

    *frame = playback_channel->free_frames->samples;
    playback_channel->free_frames = playback_channel->free_frames->next;
    *num_samples = snd_codec_frame_size(playback_channel->codec);
}

SPICE_GNUC_VISIBLE void spice_server_playback_put_samples(SpicePlaybackInstance *sin, uint32_t *samples)
{
    PlaybackChannel *playback_channel;
    AudioFrame *frame;

    frame = SPICE_CONTAINEROF(samples, AudioFrame, samples);
    playback_channel = frame->channel;
    spice_assert(playback_channel);
    if (!snd_channel_put(&playback_channel->base) ||
        sin->st->worker.connection != &playback_channel->base) {
        /* lost last reference, channel has been destroyed previously */
        spice_info("audio samples belong to a disconnected channel");
        return;
    }
    spice_assert(playback_channel->base.active);

    if (playback_channel->pending_frame) {
        snd_playback_free_frame(playback_channel, playback_channel->pending_frame);
    }
    frame->time = reds_get_mm_time();
    playback_channel->pending_frame = frame;
    snd_set_command(&playback_channel->base, SND_PLAYBACK_PCM_MASK);
    snd_playback_send(&playback_channel->base);
}

void snd_set_playback_latency(RedClient *client, uint32_t latency)
{
    SndWorker *now = workers;

    for (; now; now = now->next) {
        if (now->base_channel->type == SPICE_CHANNEL_PLAYBACK && now->connection &&
            now->connection->channel_client->client == client) {

            if (red_channel_client_test_remote_cap(now->connection->channel_client,
                SPICE_PLAYBACK_CAP_LATENCY)) {
                PlaybackChannel* playback = (PlaybackChannel*)now->connection;

                playback->latency = latency;
                snd_set_command(now->connection, SND_PLAYBACK_LATENCY_MASK);
                snd_playback_send(now->connection);
            } else {
                spice_debug("client doesn't not support SPICE_PLAYBACK_CAP_LATENCY");
            }
        }
    }
}

static int snd_desired_audio_mode(int frequency, int client_can_celt, int client_can_opus)
{
    if (! playback_compression)
        return SPICE_AUDIO_DATA_MODE_RAW;

    if (client_can_opus && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, frequency))
        return SPICE_AUDIO_DATA_MODE_OPUS;

    if (client_can_celt && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, frequency))
        return SPICE_AUDIO_DATA_MODE_CELT_0_5_1;

    return SPICE_AUDIO_DATA_MODE_RAW;
}

static void on_new_playback_channel(SndWorker *worker)
{
    PlaybackChannel *playback_channel =
        SPICE_CONTAINEROF(worker->connection, PlaybackChannel, base);
    SpicePlaybackState *st = SPICE_CONTAINEROF(worker, SpicePlaybackState, worker);

    spice_assert(playback_channel);

    snd_set_command((SndChannel *)playback_channel, SND_PLAYBACK_MODE_MASK);
    if (playback_channel->base.active) {
        snd_set_command((SndChannel *)playback_channel, SND_PLAYBACK_CTRL_MASK);
    }
    if (st->volume.volume_nchannels) {
        snd_set_command((SndChannel *)playback_channel, SND_PLAYBACK_VOLUME_MASK);
    }
    if (playback_channel->base.active) {
        reds_disable_mm_time();
    }
}

static void snd_playback_cleanup(SndChannel *channel)
{
    PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);

    if (playback_channel->base.active) {
        reds_enable_mm_time();
    }

    snd_codec_destroy(&playback_channel->codec);
}

static void snd_set_playback_peer(RedChannel *channel, RedClient *client, RedsStream *stream,
                                  int migration, int num_common_caps, uint32_t *common_caps,
                                  int num_caps, uint32_t *caps)
{
    SndWorker *worker = channel->data;
    PlaybackChannel *playback_channel;
    SpicePlaybackState *st = SPICE_CONTAINEROF(worker, SpicePlaybackState, worker);

    snd_disconnect_channel(worker->connection);

    if (!(playback_channel = (PlaybackChannel *)__new_channel(worker,
                                                              sizeof(*playback_channel),
                                                              SPICE_CHANNEL_PLAYBACK,
                                                              client,
                                                              stream,
                                                              migration,
                                                              snd_playback_send,
                                                              snd_playback_handle_message,
                                                              snd_playback_on_message_done,
                                                              snd_playback_cleanup,
                                                              common_caps, num_common_caps,
                                                              caps, num_caps))) {
        return;
    }
    worker->connection = &playback_channel->base;
    snd_playback_free_frame(playback_channel, &playback_channel->frames[0]);
    snd_playback_free_frame(playback_channel, &playback_channel->frames[1]);
    snd_playback_free_frame(playback_channel, &playback_channel->frames[2]);

    int client_can_celt = red_channel_client_test_remote_cap(playback_channel->base.channel_client,
                                          SPICE_PLAYBACK_CAP_CELT_0_5_1);
    int client_can_opus = red_channel_client_test_remote_cap(playback_channel->base.channel_client,
                                          SPICE_PLAYBACK_CAP_OPUS);
    int desired_mode = snd_desired_audio_mode(st->frequency, client_can_celt, client_can_opus);
    playback_channel->mode = SPICE_AUDIO_DATA_MODE_RAW;
    if (desired_mode != SPICE_AUDIO_DATA_MODE_RAW) {
        if (snd_codec_create(&playback_channel->codec, desired_mode, st->frequency, SND_CODEC_ENCODE) == SND_CODEC_OK) {
            playback_channel->mode = desired_mode;
        } else {
            spice_printerr("create encoder failed");
        }
    }

    if (!red_client_during_migrate_at_target(client)) {
        on_new_playback_channel(worker);
    }

    if (worker->active) {
        spice_server_playback_start(st->sin);
    }
    snd_playback_send(worker->connection);
}

static void snd_record_migrate_channel_client(RedChannelClient *rcc)
{
    SndWorker *worker;

    spice_debug(NULL);
    spice_assert(rcc->channel);
    spice_assert(rcc->channel->data);
    worker = (SndWorker *)rcc->channel->data;

    if (worker->connection) {
        spice_assert(worker->connection->channel_client == rcc);
        snd_set_command(worker->connection, SND_RECORD_MIGRATE_MASK);
        snd_record_send(worker->connection);
    }
}

SPICE_GNUC_VISIBLE void spice_server_record_set_volume(SpiceRecordInstance *sin,
                                                uint8_t nchannels,
                                                uint16_t *volume)
{
    SpiceVolumeState *st = &sin->st->volume;
    SndChannel *channel = sin->st->worker.connection;
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);

    st->volume_nchannels = nchannels;
    free(st->volume);
    st->volume = spice_memdup(volume, sizeof(uint16_t) * nchannels);

    if (!channel || nchannels == 0)
        return;

    snd_record_send_volume(record_channel);
}

SPICE_GNUC_VISIBLE void spice_server_record_set_mute(SpiceRecordInstance *sin, uint8_t mute)
{
    SpiceVolumeState *st = &sin->st->volume;
    SndChannel *channel = sin->st->worker.connection;
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);

    st->mute = mute;

    if (!channel)
        return;

    snd_record_send_mute(record_channel);
}

SPICE_GNUC_VISIBLE void spice_server_record_start(SpiceRecordInstance *sin)
{
    SndChannel *channel = sin->st->worker.connection;
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);

    sin->st->worker.active = 1;
    if (!channel)
        return;
    spice_assert(!record_channel->base.active);
    record_channel->base.active = TRUE;
    record_channel->read_pos = record_channel->write_pos = 0;   //todo: improve by
                                                                //stream generation
    if (!record_channel->base.client_active) {
        snd_set_command(&record_channel->base, SND_RECORD_CTRL_MASK);
        snd_record_send(&record_channel->base);
    } else {
        record_channel->base.command &= ~SND_RECORD_CTRL_MASK;
    }
}

SPICE_GNUC_VISIBLE void spice_server_record_stop(SpiceRecordInstance *sin)
{
    SndChannel *channel = sin->st->worker.connection;
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);

    sin->st->worker.active = 0;
    if (!channel)
        return;
    spice_assert(record_channel->base.active);
    record_channel->base.active = FALSE;
    if (record_channel->base.client_active) {
        snd_set_command(&record_channel->base, SND_RECORD_CTRL_MASK);
        snd_record_send(&record_channel->base);
    } else {
        record_channel->base.command &= ~SND_RECORD_CTRL_MASK;
    }
}

SPICE_GNUC_VISIBLE uint32_t spice_server_record_get_samples(SpiceRecordInstance *sin,
                                                            uint32_t *samples, uint32_t bufsize)
{
    SndChannel *channel = sin->st->worker.connection;
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);
    uint32_t read_pos;
    uint32_t now;
    uint32_t len;

    if (!channel)
        return 0;
    spice_assert(record_channel->base.active);

    if (record_channel->write_pos < RECORD_SAMPLES_SIZE / 2) {
        return 0;
    }

    len = MIN(record_channel->write_pos - record_channel->read_pos, bufsize);

    if (len < bufsize) {
        SndWorker *worker = record_channel->base.worker;
        snd_receive(record_channel);
        if (!worker->connection) {
            return 0;
        }
        len = MIN(record_channel->write_pos - record_channel->read_pos, bufsize);
    }

    read_pos = record_channel->read_pos % RECORD_SAMPLES_SIZE;
    record_channel->read_pos += len;
    now = MIN(len, RECORD_SAMPLES_SIZE - read_pos);
    memcpy(samples, &record_channel->samples[read_pos], now * 4);
    if (now < len) {
        memcpy(samples + now, record_channel->samples, (len - now) * 4);
    }
    return len;
}

SPICE_GNUC_VISIBLE uint32_t spice_server_get_best_playback_rate(SpicePlaybackInstance *sin)
{
    int client_can_opus = TRUE;
    if (sin && sin->st->worker.connection) {
        SndChannel *channel = sin->st->worker.connection;
        PlaybackChannel *playback_channel = SPICE_CONTAINEROF(channel, PlaybackChannel, base);
        client_can_opus = red_channel_client_test_remote_cap(playback_channel->base.channel_client,
                                          SPICE_PLAYBACK_CAP_OPUS);
    }

    if (client_can_opus && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, SND_CODEC_ANY_FREQUENCY))
        return SND_CODEC_OPUS_PLAYBACK_FREQ;

    return SND_CODEC_CELT_PLAYBACK_FREQ;
}

SPICE_GNUC_VISIBLE void spice_server_set_playback_rate(SpicePlaybackInstance *sin, uint32_t frequency)
{
    RedChannel *channel = sin->st->worker.base_channel;
    sin->st->frequency = frequency;
    if (channel && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, frequency))
        red_channel_set_cap(channel, SPICE_PLAYBACK_CAP_OPUS);
}

SPICE_GNUC_VISIBLE uint32_t spice_server_get_best_record_rate(SpiceRecordInstance *sin)
{
    int client_can_opus = TRUE;
    if (sin && sin->st->worker.connection) {
        SndChannel *channel = sin->st->worker.connection;
        RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);
        client_can_opus = red_channel_client_test_remote_cap(record_channel->base.channel_client,
                                          SPICE_RECORD_CAP_OPUS);
    }

    if (client_can_opus && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, SND_CODEC_ANY_FREQUENCY))
        return SND_CODEC_OPUS_PLAYBACK_FREQ;

    return SND_CODEC_CELT_PLAYBACK_FREQ;
}

SPICE_GNUC_VISIBLE void spice_server_set_record_rate(SpiceRecordInstance *sin, uint32_t frequency)
{
    RedChannel *channel = sin->st->worker.base_channel;
    sin->st->frequency = frequency;
    if (channel && snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, frequency))
        red_channel_set_cap(channel, SPICE_RECORD_CAP_OPUS);
}

static void on_new_record_channel(SndWorker *worker)
{
    RecordChannel *record_channel = (RecordChannel *)worker->connection;
    SpiceRecordState *st = SPICE_CONTAINEROF(worker, SpiceRecordState, worker);

    spice_assert(record_channel);

    if (st->volume.volume_nchannels) {
        snd_set_command((SndChannel *)record_channel, SND_RECORD_VOLUME_MASK);
    }
    if (record_channel->base.active) {
        snd_set_command((SndChannel *)record_channel, SND_RECORD_CTRL_MASK);
    }
}

static void snd_record_cleanup(SndChannel *channel)
{
    RecordChannel *record_channel = SPICE_CONTAINEROF(channel, RecordChannel, base);
    snd_codec_destroy(&record_channel->codec);
}

static void snd_set_record_peer(RedChannel *channel, RedClient *client, RedsStream *stream,
                                int migration, int num_common_caps, uint32_t *common_caps,
                                int num_caps, uint32_t *caps)
{
    SndWorker *worker = channel->data;
    RecordChannel *record_channel;
    SpiceRecordState *st = SPICE_CONTAINEROF(worker, SpiceRecordState, worker);

    snd_disconnect_channel(worker->connection);

    if (!(record_channel = (RecordChannel *)__new_channel(worker,
                                                          sizeof(*record_channel),
                                                          SPICE_CHANNEL_RECORD,
                                                          client,
                                                          stream,
                                                          migration,
                                                          snd_record_send,
                                                          snd_record_handle_message,
                                                          snd_record_on_message_done,
                                                          snd_record_cleanup,
                                                          common_caps, num_common_caps,
                                                          caps, num_caps))) {
        return;
    }

    record_channel->mode = SPICE_AUDIO_DATA_MODE_RAW;

    worker->connection = &record_channel->base;

    on_new_record_channel(worker);
    if (worker->active) {
        spice_server_record_start(st->sin);
    }
    snd_record_send(worker->connection);
}

static void snd_playback_migrate_channel_client(RedChannelClient *rcc)
{
    SndWorker *worker;

    spice_assert(rcc->channel);
    spice_assert(rcc->channel->data);
    worker = (SndWorker *)rcc->channel->data;
    spice_debug(NULL);

    if (worker->connection) {
        spice_assert(worker->connection->channel_client == rcc);
        snd_set_command(worker->connection, SND_PLAYBACK_MIGRATE_MASK);
        snd_playback_send(worker->connection);
    }
}

static void add_worker(SndWorker *worker)
{
    worker->next = workers;
    workers = worker;
}

static void remove_worker(SndWorker *worker)
{
    SndWorker **now = &workers;
    while (*now) {
        if (*now == worker) {
            *now = worker->next;
            return;
        }
        now = &(*now)->next;
    }
    spice_printerr("not found");
}

void snd_attach_playback(SpicePlaybackInstance *sin)
{
    SndWorker *playback_worker;
    RedChannel *channel;
    ClientCbs client_cbs = { NULL, };

    sin->st = spice_new0(SpicePlaybackState, 1);
    sin->st->sin = sin;
    playback_worker = &sin->st->worker;
    sin->st->frequency = SND_CODEC_CELT_PLAYBACK_FREQ; /* Default to the legacy rate */

    // TODO: Make RedChannel base of worker? instead of assigning it to channel->data
    channel = red_channel_create_dummy(sizeof(RedChannel), SPICE_CHANNEL_PLAYBACK, 0);

    channel->data = playback_worker;
    client_cbs.connect = snd_set_playback_peer;
    client_cbs.disconnect = snd_disconnect_channel_client;
    client_cbs.migrate = snd_playback_migrate_channel_client;
    red_channel_register_client_cbs(channel, &client_cbs);
    red_channel_set_data(channel, playback_worker);

    if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, SND_CODEC_ANY_FREQUENCY))
        red_channel_set_cap(channel, SPICE_PLAYBACK_CAP_CELT_0_5_1);

    red_channel_set_cap(channel, SPICE_PLAYBACK_CAP_VOLUME);

    playback_worker->base_channel = channel;
    add_worker(playback_worker);
    reds_register_channel(playback_worker->base_channel);
}

void snd_attach_record(SpiceRecordInstance *sin)
{
    SndWorker *record_worker;
    RedChannel *channel;
    ClientCbs client_cbs = { NULL, };

    sin->st = spice_new0(SpiceRecordState, 1);
    sin->st->sin = sin;
    record_worker = &sin->st->worker;
    sin->st->frequency = SND_CODEC_CELT_PLAYBACK_FREQ; /* Default to the legacy rate */

    // TODO: Make RedChannel base of worker? instead of assigning it to channel->data
    channel = red_channel_create_dummy(sizeof(RedChannel), SPICE_CHANNEL_RECORD, 0);

    channel->data = record_worker;
    client_cbs.connect = snd_set_record_peer;
    client_cbs.disconnect = snd_disconnect_channel_client;
    client_cbs.migrate = snd_record_migrate_channel_client;
    red_channel_register_client_cbs(channel, &client_cbs);
    red_channel_set_data(channel, record_worker);
    if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, SND_CODEC_ANY_FREQUENCY))
        red_channel_set_cap(channel, SPICE_RECORD_CAP_CELT_0_5_1);
    red_channel_set_cap(channel, SPICE_RECORD_CAP_VOLUME);

    record_worker->base_channel = channel;
    add_worker(record_worker);
    reds_register_channel(record_worker->base_channel);
}

static void snd_detach_common(SndWorker *worker)
{
    if (!worker) {
        return;
    }
    remove_worker(worker);
    snd_disconnect_channel(worker->connection);
    reds_unregister_channel(worker->base_channel);
    red_channel_destroy(worker->base_channel);
}

static void spice_playback_state_free(SpicePlaybackState *st)
{
    free(st->volume.volume);
    free(st);
}

void snd_detach_playback(SpicePlaybackInstance *sin)
{
    snd_detach_common(&sin->st->worker);
    spice_playback_state_free(sin->st);
}

static void spice_record_state_free(SpiceRecordState *st)
{
    free(st->volume.volume);
    free(st);
}

void snd_detach_record(SpiceRecordInstance *sin)
{
    snd_detach_common(&sin->st->worker);
    spice_record_state_free(sin->st);
}

void snd_set_playback_compression(int on)
{
    SndWorker *now = workers;

    playback_compression = !!on;

    for (; now; now = now->next) {
        if (now->base_channel->type == SPICE_CHANNEL_PLAYBACK && now->connection) {
            PlaybackChannel* playback = (PlaybackChannel*)now->connection;
            SpicePlaybackState *st = SPICE_CONTAINEROF(now, SpicePlaybackState, worker);
            int client_can_celt = red_channel_client_test_remote_cap(playback->base.channel_client,
                                    SPICE_PLAYBACK_CAP_CELT_0_5_1);
            int client_can_opus = red_channel_client_test_remote_cap(playback->base.channel_client,
                                    SPICE_PLAYBACK_CAP_OPUS);
            int desired_mode = snd_desired_audio_mode(st->frequency, client_can_opus, client_can_celt);
            if (playback->mode != desired_mode) {
                playback->mode = desired_mode;
                snd_set_command(now->connection, SND_PLAYBACK_MODE_MASK);
            }
        }
    }
}
