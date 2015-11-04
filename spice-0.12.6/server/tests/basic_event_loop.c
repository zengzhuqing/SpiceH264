#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <glib.h>

#include "spice/macros.h"
#include "common/ring.h"
#include "test_util.h"
#include "basic_event_loop.h"

int debug = 0;

#define DPRINTF(x, format, ...) { \
    if (x <= debug) { \
        printf("%s: " format "\n" , __FUNCTION__, ## __VA_ARGS__); \
    } \
}

#define NOT_IMPLEMENTED printf("%s not implemented\n", __func__);


struct SpiceTimer {
    SpiceTimerFunc func;
    void *opaque;
    guint source_id;
};

static SpiceTimer* timer_add(SpiceTimerFunc func, void *opaque)
{
    SpiceTimer *timer = calloc(sizeof(SpiceTimer), 1);

    timer->func = func;
    timer->opaque = opaque;

    return timer;
}

static gboolean timer_func(gpointer user_data)
{
    SpiceTimer *timer = user_data;

    timer->source_id = 0;
    timer->func(timer->opaque);
    /* timer might be free after func(), don't touch */

    return FALSE;
}

static void timer_cancel(SpiceTimer *timer)
{
    if (timer->source_id == 0)
        return;

    g_source_remove(timer->source_id);
    timer->source_id = 0;
}

static void timer_start(SpiceTimer *timer, uint32_t ms)
{
    timer_cancel(timer);

    timer->source_id = g_timeout_add(ms, timer_func, timer);
}

static void timer_remove(SpiceTimer *timer)
{
    timer_cancel(timer);
    g_free(timer);
}

struct SpiceWatch {
    void *opaque;
    guint source_id;
    GIOChannel *channel;
    SpiceWatchFunc func;
};

static GIOCondition spice_event_to_condition(int event_mask)
{
    GIOCondition condition = 0;

    if (event_mask & SPICE_WATCH_EVENT_READ)
        condition |= G_IO_IN;
    if (event_mask & SPICE_WATCH_EVENT_WRITE)
        condition |= G_IO_OUT;

    return condition;
}

static int condition_to_spice_event(GIOCondition condition)
{
    int event = 0;

    if (condition & G_IO_IN)
        event |= SPICE_WATCH_EVENT_READ;
    if (condition & G_IO_OUT)
        event |= SPICE_WATCH_EVENT_WRITE;

    return event;
}

static gboolean watch_func(GIOChannel *source, GIOCondition condition,
                           gpointer data)
{
    SpiceWatch *watch = data;
    int fd = g_io_channel_unix_get_fd(source);

    watch->func(fd, condition_to_spice_event(condition), watch->opaque);

    return TRUE;
}

static SpiceWatch *watch_add(int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    SpiceWatch *watch;
    GIOCondition condition = spice_event_to_condition(event_mask);

    watch = g_new(SpiceWatch, 1);
    watch->channel = g_io_channel_unix_new(fd);
    watch->source_id = g_io_add_watch(watch->channel, condition, watch_func, watch);
    watch->func = func;
    watch->opaque = opaque;

    return watch;
}

static void watch_update_mask(SpiceWatch *watch, int event_mask)
{
    GIOCondition condition = spice_event_to_condition(event_mask);

    g_source_remove(watch->source_id);
    if (condition != 0)
        watch->source_id = g_io_add_watch(watch->channel, condition, watch_func, watch);
}

static void watch_remove(SpiceWatch *watch)
{
    g_source_remove(watch->source_id);
    g_io_channel_unref(watch->channel);
    g_free(watch);
}

static void channel_event(int event, SpiceChannelEventInfo *info)
{
    DPRINTF(0, "channel event con, type, id, event: %d, %d, %d, %d",
            info->connection_id, info->type, info->id, event);
}

void basic_event_loop_mainloop(void)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

static void ignore_sigpipe(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
}

static SpiceCoreInterface core = {
    .base = {
        .major_version = SPICE_INTERFACE_CORE_MAJOR,
        .minor_version = SPICE_INTERFACE_CORE_MINOR,
    },
    .timer_add = timer_add,
    .timer_start = timer_start,
    .timer_cancel = timer_cancel,
    .timer_remove = timer_remove,
    .watch_add = watch_add,
    .watch_update_mask = watch_update_mask,
    .watch_remove = watch_remove,
    .channel_event = channel_event,
};

SpiceCoreInterface *basic_event_loop_init(void)
{
    ignore_sigpipe();
    return &core;
}
