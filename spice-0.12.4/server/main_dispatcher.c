#include <config.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "red_common.h"
#include "dispatcher.h"
#include "main_dispatcher.h"
#include "red_channel.h"
#include "reds.h"

/*
 * Main Dispatcher
 * ===============
 *
 * Communication channel between any non main thread and the main thread.
 *
 * The main thread is that from which spice_server_init is called.
 *
 * Messages are single sized, sent from the non-main thread to the main-thread.
 * No acknowledge is sent back. This prevents a possible deadlock with the main
 * thread already waiting on a response for the existing red_dispatcher used
 * by the worker thread.
 *
 * All events have three functions:
 * main_dispatcher_<event_name> - non static, public function
 * main_dispatcher_self_<event_name> - handler for main thread
 * main_dispatcher_handle_<event_name> - handler for callback from main thread
 *   seperate from self because it may send an ack or do other work in the future.
 */

typedef struct {
    Dispatcher base;
    SpiceCoreInterface *core;
} MainDispatcher;

MainDispatcher main_dispatcher;

enum {
    MAIN_DISPATCHER_CHANNEL_EVENT = 0,
    MAIN_DISPATCHER_MIGRATE_SEAMLESS_DST_COMPLETE,
    MAIN_DISPATCHER_SET_MM_TIME_LATENCY,
    MAIN_DISPATCHER_CLIENT_DISCONNECT,

    MAIN_DISPATCHER_NUM_MESSAGES
};

typedef struct MainDispatcherChannelEventMessage {
    int event;
    SpiceChannelEventInfo *info;
} MainDispatcherChannelEventMessage;

typedef struct MainDispatcherMigrateSeamlessDstCompleteMessage {
    RedClient *client;
} MainDispatcherMigrateSeamlessDstCompleteMessage;

typedef struct MainDispatcherMmTimeLatencyMessage {
    RedClient *client;
    uint32_t latency;
} MainDispatcherMmTimeLatencyMessage;

typedef struct MainDispatcherClientDisconnectMessage {
    RedClient *client;
} MainDispatcherClientDisconnectMessage;

/* channel_event - calls core->channel_event, must be done in main thread */
static void main_dispatcher_self_handle_channel_event(
                                                int event,
                                                SpiceChannelEventInfo *info)
{
    reds_handle_channel_event(event, info);
}

static void main_dispatcher_handle_channel_event(void *opaque,
                                                 void *payload)
{
    MainDispatcherChannelEventMessage *channel_event = payload;

    main_dispatcher_self_handle_channel_event(channel_event->event,
                                              channel_event->info);
}

void main_dispatcher_channel_event(int event, SpiceChannelEventInfo *info)
{
    MainDispatcherChannelEventMessage msg = {0,};

    if (pthread_self() == main_dispatcher.base.self) {
        main_dispatcher_self_handle_channel_event(event, info);
        return;
    }
    msg.event = event;
    msg.info = info;
    dispatcher_send_message(&main_dispatcher.base, MAIN_DISPATCHER_CHANNEL_EVENT,
                            &msg);
}


static void main_dispatcher_handle_migrate_complete(void *opaque,
                                                    void *payload)
{
    MainDispatcherMigrateSeamlessDstCompleteMessage *mig_complete = payload;

    reds_on_client_seamless_migrate_complete(mig_complete->client);
    red_client_unref(mig_complete->client);
}

static void main_dispatcher_handle_mm_time_latency(void *opaque,
                                                   void *payload)
{
    MainDispatcherMmTimeLatencyMessage *msg = payload;
    reds_set_client_mm_time_latency(msg->client, msg->latency);
    red_client_unref(msg->client);
}

static void main_dispatcher_handle_client_disconnect(void *opaque,
                                                     void *payload)
{
    MainDispatcherClientDisconnectMessage *msg = payload;

    spice_debug("client=%p", msg->client);
    reds_client_disconnect(msg->client);
    red_client_unref(msg->client);
}

void main_dispatcher_seamless_migrate_dst_complete(RedClient *client)
{
    MainDispatcherMigrateSeamlessDstCompleteMessage msg;

    if (pthread_self() == main_dispatcher.base.self) {
        reds_on_client_seamless_migrate_complete(client);
        return;
    }

    msg.client = red_client_ref(client);
    dispatcher_send_message(&main_dispatcher.base, MAIN_DISPATCHER_MIGRATE_SEAMLESS_DST_COMPLETE,
                            &msg);
}

void main_dispatcher_set_mm_time_latency(RedClient *client, uint32_t latency)
{
    MainDispatcherMmTimeLatencyMessage msg;

    if (pthread_self() == main_dispatcher.base.self) {
        reds_set_client_mm_time_latency(client, latency);
        return;
    }

    msg.client = red_client_ref(client);
    msg.latency = latency;
    dispatcher_send_message(&main_dispatcher.base, MAIN_DISPATCHER_SET_MM_TIME_LATENCY,
                            &msg);
}

void main_dispatcher_client_disconnect(RedClient *client)
{
    MainDispatcherClientDisconnectMessage msg;

    if (!client->disconnecting) {
        spice_debug("client %p", client);
        msg.client = red_client_ref(client);
        dispatcher_send_message(&main_dispatcher.base, MAIN_DISPATCHER_CLIENT_DISCONNECT,
                                &msg);
    } else {
        spice_debug("client %p already during disconnection", client);
    }
}

static void dispatcher_handle_read(int fd, int event, void *opaque)
{
    Dispatcher *dispatcher = opaque;

    dispatcher_handle_recv_read(dispatcher);
}

/*
 * FIXME:
 * Reds routines shouldn't be exposed. Instead reds.c should register the callbacks,
 * and the corresponding operations should be made only via main_dispatcher.
 */
void main_dispatcher_init(SpiceCoreInterface *core)
{
    memset(&main_dispatcher, 0, sizeof(main_dispatcher));
    main_dispatcher.core = core;
    dispatcher_init(&main_dispatcher.base, MAIN_DISPATCHER_NUM_MESSAGES, &main_dispatcher.base);
    core->watch_add(main_dispatcher.base.recv_fd, SPICE_WATCH_EVENT_READ,
                    dispatcher_handle_read, &main_dispatcher.base);
    dispatcher_register_handler(&main_dispatcher.base, MAIN_DISPATCHER_CHANNEL_EVENT,
                                main_dispatcher_handle_channel_event,
                                sizeof(MainDispatcherChannelEventMessage), 0 /* no ack */);
    dispatcher_register_handler(&main_dispatcher.base, MAIN_DISPATCHER_MIGRATE_SEAMLESS_DST_COMPLETE,
                                main_dispatcher_handle_migrate_complete,
                                sizeof(MainDispatcherMigrateSeamlessDstCompleteMessage), 0 /* no ack */);
    dispatcher_register_handler(&main_dispatcher.base, MAIN_DISPATCHER_SET_MM_TIME_LATENCY,
                                main_dispatcher_handle_mm_time_latency,
                                sizeof(MainDispatcherMmTimeLatencyMessage), 0 /* no ack */);
    dispatcher_register_handler(&main_dispatcher.base, MAIN_DISPATCHER_CLIENT_DISCONNECT,
                                main_dispatcher_handle_client_disconnect,
                                sizeof(MainDispatcherClientDisconnectMessage), 0 /* no ack */);
}
