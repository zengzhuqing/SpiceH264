/*
 *  Copyright (C) 2009-2014 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPICE_AUDIO_H_
#define SPICE_AUDIO_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include "spice-core.h"

/* sound interfaces */

#define SPICE_INTERFACE_PLAYBACK "playback"
#define SPICE_INTERFACE_PLAYBACK_MAJOR 1
#define SPICE_INTERFACE_PLAYBACK_MINOR 3
typedef struct SpicePlaybackInterface SpicePlaybackInterface;
typedef struct SpicePlaybackInstance SpicePlaybackInstance;
typedef struct SpicePlaybackState SpicePlaybackState;

enum {
    SPICE_INTERFACE_AUDIO_FMT_S16 = 1,
};

#define SPICE_INTERFACE_PLAYBACK_FREQ  44100
#define SPICE_INTERFACE_PLAYBACK_CHAN  2
#define SPICE_INTERFACE_PLAYBACK_FMT   SPICE_INTERFACE_AUDIO_FMT_S16

struct SpicePlaybackInterface {
    SpiceBaseInterface base;
};

struct SpicePlaybackInstance {
    SpiceBaseInstance  base;
    SpicePlaybackState *st;
};

void spice_server_playback_start(SpicePlaybackInstance *sin);
void spice_server_playback_stop(SpicePlaybackInstance *sin);
void spice_server_playback_get_buffer(SpicePlaybackInstance *sin,
                                      uint32_t **samples, uint32_t *nsamples);
void spice_server_playback_put_samples(SpicePlaybackInstance *sin,
                                       uint32_t *samples);
void spice_server_playback_set_volume(SpicePlaybackInstance *sin,
                                      uint8_t nchannels, uint16_t *volume);
void spice_server_playback_set_mute(SpicePlaybackInstance *sin, uint8_t mute);

#define SPICE_INTERFACE_RECORD "record"
#define SPICE_INTERFACE_RECORD_MAJOR 2
#define SPICE_INTERFACE_RECORD_MINOR 3
typedef struct SpiceRecordInterface SpiceRecordInterface;
typedef struct SpiceRecordInstance SpiceRecordInstance;
typedef struct SpiceRecordState SpiceRecordState;

#define SPICE_INTERFACE_RECORD_FREQ  44100
#define SPICE_INTERFACE_RECORD_CHAN  2
#define SPICE_INTERFACE_RECORD_FMT   SPICE_INTERFACE_AUDIO_FMT_S16

struct SpiceRecordInterface {
    SpiceBaseInterface base;
};

struct SpiceRecordInstance {
    SpiceBaseInstance base;
    SpiceRecordState  *st;
};

void spice_server_record_start(SpiceRecordInstance *sin);
void spice_server_record_stop(SpiceRecordInstance *sin);
uint32_t spice_server_record_get_samples(SpiceRecordInstance *sin,
                                         uint32_t *samples, uint32_t bufsize);
void spice_server_record_set_volume(SpiceRecordInstance *sin,
                                    uint8_t nchannels, uint16_t *volume);
void spice_server_record_set_mute(SpiceRecordInstance *sin, uint8_t mute);

uint32_t spice_server_get_best_playback_rate(SpicePlaybackInstance *sin);
void     spice_server_set_playback_rate(SpicePlaybackInstance *sin, uint32_t frequency);
uint32_t spice_server_get_best_record_rate(SpiceRecordInstance *sin);
void     spice_server_set_record_rate(SpiceRecordInstance *sin, uint32_t frequency);

#endif /* SPICE_AUDIO_H_ */
