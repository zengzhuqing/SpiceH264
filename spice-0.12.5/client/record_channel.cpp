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

#include "common.h"
#include "red_client.h"
#include "audio_channels.h"
#include "audio_devices.h"

#define NUM_SAMPLES_MESSAGES 4


static uint32_t get_mm_time()
{
    return uint32_t(Platform::get_monolithic_time() / (1000 * 1000));
}

class RecordSamplesMessage: public RedChannel::OutMessage {
public:
    RecordSamplesMessage(RecordChannel& channel);
    virtual ~RecordSamplesMessage();

    virtual RedPeer::OutMessage& peer_message() { return *_message;}
    virtual void release();

private:
    RecordChannel& _channel;
    RedPeer::OutMessage *_message;
};

RecordSamplesMessage::RecordSamplesMessage(RecordChannel& channel)
    : _channel (channel)
    , _message (new Message(SPICE_MSGC_RECORD_DATA))
{
}

RecordSamplesMessage::~RecordSamplesMessage()
{
    delete _message;
}

void RecordSamplesMessage::release()
{
    _channel.release_message(this);
}

class RecordHandler: public MessageHandlerImp<RecordChannel, SPICE_CHANNEL_RECORD> {
public:
    RecordHandler(RecordChannel& channel)
        : MessageHandlerImp<RecordChannel, SPICE_CHANNEL_RECORD>(channel) {}
};

RecordChannel::RecordChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_RECORD, id, new RecordHandler(*this))
    , _wave_recorder (NULL)
    , _mode (SPICE_AUDIO_DATA_MODE_INVALID)
    , _codec(NULL)
{
    for (int i = 0; i < NUM_SAMPLES_MESSAGES; i++) {
        _messages.push_front(new RecordSamplesMessage(*this));
    }

    RecordHandler* handler = static_cast<RecordHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_MIGRATE, &RecordChannel::handle_migrate);
    handler->set_handler(SPICE_MSG_SET_ACK, &RecordChannel::handle_set_ack);
    handler->set_handler(SPICE_MSG_PING, &RecordChannel::handle_ping);
    handler->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &RecordChannel::handle_wait_for_channels);
    handler->set_handler(SPICE_MSG_DISCONNECTING, &RecordChannel::handle_disconnect);
    handler->set_handler(SPICE_MSG_NOTIFY, &RecordChannel::handle_notify);

    handler->set_handler(SPICE_MSG_RECORD_START, &RecordChannel::handle_start);

    if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, SND_CODEC_ANY_FREQUENCY))
        set_capability(SPICE_RECORD_CAP_CELT_0_5_1);
    if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, SND_CODEC_ANY_FREQUENCY))
        set_capability(SPICE_RECORD_CAP_OPUS);
}

RecordChannel::~RecordChannel(void)
{
    while (!_messages.empty()) {
        RecordSamplesMessage *mes;
        mes = *_messages.begin();
        _messages.pop_front();
        delete mes;
    }
    clear();
}

bool RecordChannel::abort(void)
{
    return (!_wave_recorder || _wave_recorder->abort()) && RedChannel::abort();
}

void RecordChannel::set_desired_mode(int frequency)
{
    if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, frequency) &&
      test_capability(SPICE_RECORD_CAP_OPUS))
          _mode = SPICE_AUDIO_DATA_MODE_OPUS;
    else if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, frequency) &&
      test_capability(SPICE_RECORD_CAP_CELT_0_5_1))
          _mode = SPICE_AUDIO_DATA_MODE_CELT_0_5_1;
      else
          _mode = SPICE_AUDIO_DATA_MODE_RAW;
}

void RecordChannel::send_record_mode()
{
    Message* message = new Message(SPICE_MSGC_RECORD_MODE);
    SpiceMsgcRecordMode mode;
    mode.time = get_mm_time();
    mode.mode = _mode;
    _marshallers->msgc_record_mode(message->marshaller(), &mode);
    post_message(message);
}

void RecordChannel::on_disconnect()
{
    clear();
}

void RecordChannel::send_start_mark()
{
    Message* message = new Message(SPICE_MSGC_RECORD_START_MARK);
    SpiceMsgcRecordStartMark start_mark;
    start_mark.time = get_mm_time();
    _marshallers->msgc_record_start_mark(message->marshaller(), &start_mark);
    post_message(message);
}

void RecordChannel::handle_start(RedPeer::InMessage* message)
{
    RecordHandler* handler = static_cast<RecordHandler*>(get_message_handler());
    SpiceMsgRecordStart* start = (SpiceMsgRecordStart*)message->data();

    handler->set_handler(SPICE_MSG_RECORD_START, NULL);
    handler->set_handler(SPICE_MSG_RECORD_STOP, &RecordChannel::handle_stop);
    ASSERT(!_wave_recorder);

    // for now support only one setting
    if (start->format != SPICE_AUDIO_FMT_S16) {
        THROW("unexpected format");
    }
    if (start->channels != 2) {
        THROW("unexpected number of channels");
    }

    set_desired_mode(start->frequency);

    int frame_size = SND_CODEC_MAX_FRAME_SIZE;
    if (_mode != SPICE_AUDIO_DATA_MODE_RAW) {
        if (snd_codec_create(&_codec, _mode, start->frequency, SND_CODEC_ENCODE) != SND_CODEC_OK)
            THROW("create encoder failed");
        frame_size = snd_codec_frame_size(_codec);
    }

    int bits_per_sample = 16;
    try {
        _wave_recorder = Platform::create_recorder(*this, start->frequency,
                                                   bits_per_sample,
                                                   start->channels,
                                                   frame_size);
    } catch (...) {
        LOG_WARN("create recorder failed");
        return;
    }

    _frame_bytes = frame_size * bits_per_sample * start->channels / 8;

    send_record_mode();
    send_start_mark();
    _wave_recorder->start();
}

void RecordChannel::clear()
{
    if (_wave_recorder) {
        _wave_recorder->stop();
        delete _wave_recorder;
        _wave_recorder = NULL;
    }
    snd_codec_destroy(&_codec);
}

void RecordChannel::handle_stop(RedPeer::InMessage* message)
{
    RecordHandler* handler = static_cast<RecordHandler*>(get_message_handler());
    handler->set_handler(SPICE_MSG_RECORD_START, &RecordChannel::handle_start);
    handler->set_handler(SPICE_MSG_RECORD_STOP, NULL);
    if (!_wave_recorder) {
        return;
    }
    clear();
}

RecordSamplesMessage* RecordChannel::get_message()
{
    Lock lock(_messages_lock);
    if (_messages.empty()) {
        return NULL;
    }

    RecordSamplesMessage* ret = *_messages.begin();
    _messages.pop_front();
    return ret;
}

void RecordChannel::release_message(RecordSamplesMessage *message)
{
    Lock lock(_messages_lock);
    _messages.push_front(message);
}

void RecordChannel::add_event_source(EventSources::File& event_source)
{
    get_process_loop().add_file(event_source);
}

void RecordChannel::remove_event_source(EventSources::File& event_source)
{
    get_process_loop().remove_file(event_source);
}

void RecordChannel::add_event_source(EventSources::Trigger& event_source)
{
    get_process_loop().add_trigger(event_source);
}

void RecordChannel::remove_event_source(EventSources::Trigger& event_source)
{
    get_process_loop().remove_trigger(event_source);
}

void RecordChannel::push_frame(uint8_t *frame)
{
    RecordSamplesMessage *message;
    if (!(message = get_message())) {
        DBG(0, "blocked");
        return;
    }
    int n;


    if (_mode == SPICE_AUDIO_DATA_MODE_RAW) {
        n = _frame_bytes;
    } else {
        n = sizeof(compressed_buf);
        if (snd_codec_encode(_codec, frame, _frame_bytes, compressed_buf, &n) != SND_CODEC_OK)
            THROW("encode failed");
        frame = compressed_buf;
    }

    RedPeer::OutMessage& peer_message = message->peer_message();
    peer_message.reset(SPICE_MSGC_RECORD_DATA);
    SpiceMsgcRecordPacket packet;
    packet.time = get_mm_time();
    _marshallers->msgc_record_data(peer_message.marshaller(), &packet);
    spice_marshaller_add(peer_message.marshaller(), frame, n);
    post_message(message);
}

class RecordFactory: public ChannelFactory {
public:
    RecordFactory() : ChannelFactory(SPICE_CHANNEL_RECORD) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new RecordChannel(client, id);
    }
};

static RecordFactory factory;

ChannelFactory& RecordChannel::Factory()
{
    return factory;
}
