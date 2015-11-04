/* Replay a previously recorded file (via SPICE_WORKER_RECORD_FILENAME)
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glib.h>
#include <pthread.h>

#include <spice/macros.h>
#include "red_replay_qxl.h"
#include "test_display_base.h"
#include "common/log.h"

static SpiceCoreInterface *core;
static SpiceServer *server;
static SpiceReplay *replay;
static QXLWorker *qxl_worker = NULL;
static gboolean started = FALSE;
static QXLInstance display_sin = { 0, };
static gint slow = 0;
static pid_t client_pid;
static GMainLoop *loop = NULL;
static GAsyncQueue *aqueue = NULL;
static long total_size;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static guint fill_source_id = 0;


#define MEM_SLOT_GROUP_ID 0

/* Parts cribbed from spice-display.h/.c/qxl.c */

static QXLDevMemSlot slot = {
.slot_group_id = MEM_SLOT_GROUP_ID,
.slot_id = 0,
.generation = 0,
.virt_start = 0,
.virt_end = ~0,
.addr_delta = 0,
.qxl_ram_size = ~0,
};

static void attach_worker(QXLInstance *qin, QXLWorker *_qxl_worker)
{
    static int count = 0;
    if (++count > 1) {
        g_warning("%s ignored\n", __func__);
        return;
    }
    g_debug("%s\n", __func__);
    qxl_worker = _qxl_worker;
    spice_qxl_add_memslot(qin, &slot);
    spice_server_vm_start(server);
}

static void set_compression_level(QXLInstance *qin, int level)
{
    g_debug("%s\n", __func__);
}

static void set_mm_time(QXLInstance *qin, uint32_t mm_time)
{
}

// same as qemu/ui/spice-display.h
#define MAX_SURFACE_NUM 1024

static void get_init_info(QXLInstance *qin, QXLDevInitInfo *info)
{
    bzero(info, sizeof(*info));
    info->num_memslots = 1;
    info->num_memslots_groups = 1;
    info->memslot_id_bits = 1;
    info->memslot_gen_bits = 1;
    info->n_surfaces = MAX_SURFACE_NUM;
}

static gboolean fill_queue_idle(gpointer user_data)
{
    gboolean keep = FALSE;

    while (g_async_queue_length(aqueue) < 50) {
        QXLCommandExt *cmd = spice_replay_next_cmd(replay, qxl_worker);
        if (!cmd) {
            g_async_queue_push(aqueue, GINT_TO_POINTER(-1));
            goto end;
        }

        if (slow)
            g_usleep(slow);

        g_async_queue_push(aqueue, cmd);
    }

end:
    if (!keep) {
        pthread_mutex_lock(&mutex);
        fill_source_id = 0;
        pthread_mutex_unlock(&mutex);
    }
    spice_qxl_wakeup(&display_sin);

    return keep;
}

static void fill_queue(void)
{
    pthread_mutex_lock(&mutex);

    if (!started)
        goto end;

    if (fill_source_id != 0)
        goto end;

    fill_source_id = g_idle_add(fill_queue_idle, NULL);

end:
    pthread_mutex_unlock(&mutex);
}


// called from spice_server thread (i.e. red_worker thread)
static int get_command(QXLInstance *qin, QXLCommandExt *ext)
{
    QXLCommandExt *cmd;

    if (g_async_queue_length(aqueue) == 0) {
        /* could use a gcondition ? */
        fill_queue();
        return FALSE;
    }

    cmd = g_async_queue_try_pop(aqueue);
    if (GPOINTER_TO_INT(cmd) == -1) {
        g_main_loop_quit(loop);
        return FALSE;
    }

    *ext = *cmd;

    return TRUE;
}

static int req_cmd_notification(QXLInstance *qin)
{
    if (!started)
        return TRUE;

    g_printerr("id: %d, queue length: %d",
                   fill_source_id, g_async_queue_length(aqueue));

    return TRUE;
}

static void end_replay(void)
{
    int child_status;

    /* FIXME: wait threads and end cleanly */
    spice_replay_free(replay);

    if (client_pid) {
        g_debug("kill %d", client_pid);
        kill(client_pid, SIGINT);
        waitpid(client_pid, &child_status, 0);
    }
    exit(0);
}

static void release_resource(QXLInstance *qin, struct QXLReleaseInfoExt release_info)
{
    spice_replay_free_cmd(replay, (QXLCommandExt *)release_info.info->id);
}

static int get_cursor_command(QXLInstance *qin, struct QXLCommandExt *ext)
{
    return FALSE;
}

static int req_cursor_notification(QXLInstance *qin)
{
    return TRUE;
}

static void notify_update(QXLInstance *qin, uint32_t update_id)
{
}

static int flush_resources(QXLInstance *qin)
{
    return TRUE;
}

static QXLInterface display_sif = {
    .base = {
        .type = SPICE_INTERFACE_QXL,
        .description = "replay",
        .major_version = SPICE_INTERFACE_QXL_MAJOR,
        .minor_version = SPICE_INTERFACE_QXL_MINOR
    },
    .attache_worker = attach_worker,
    .set_compression_level = set_compression_level,
    .set_mm_time = set_mm_time,
    .get_init_info = get_init_info,
    .get_command = get_command,
    .req_cmd_notification = req_cmd_notification,
    .release_resource = release_resource,
    .get_cursor_command = get_cursor_command,
    .req_cursor_notification = req_cursor_notification,
    .notify_update = notify_update,
    .flush_resources = flush_resources,
};

static void replay_channel_event(int event, SpiceChannelEventInfo *info)
{
    if (info->type == SPICE_CHANNEL_DISPLAY &&
        event == SPICE_CHANNEL_EVENT_INITIALIZED) {
        started = TRUE;
    }
}

static gboolean start_client(gchar *cmd, GError **error)
{
    gboolean retval;
    gint argc;
    gchar **argv = NULL;


    if (!g_shell_parse_argv(cmd, &argc, &argv, error))
        return FALSE;

    retval = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                           NULL, NULL, &client_pid, error);
    g_strfreev(argv);

    return retval;
}

static gboolean progress_timer(gpointer user_data)
{
    FILE *fd = user_data;
    /* it seems somehow thread safe, move to worker thread? */
    double pos = (double)ftell(fd);

    g_debug("%.2f%%", pos/total_size * 100);
    return TRUE;
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *context = NULL;
    gchar *client = NULL, **file = NULL;
    gint port = 5000, compression = SPICE_IMAGE_COMPRESSION_AUTO_GLZ;
    gboolean wait = FALSE;
    FILE *fd;

    GOptionEntry entries[] = {
        { "client", 'c', 0, G_OPTION_ARG_STRING, &client, "Client", "CMD" },
        { "compression", 'C', 0, G_OPTION_ARG_INT, &compression, "Compression (default 2)", "INT" },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Server port (default 5000)", "PORT" },
        { "wait", 'w', 0, G_OPTION_ARG_NONE, &wait, "Wait for client", NULL },
        { "slow", 's', 0, G_OPTION_ARG_INT, &slow, "Slow down replay", NULL },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file, "replay file", "FILE" },
        { NULL }
    };

    context = g_option_context_new("- replay spice server recording");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Option parsing failed: %s\n", error->message);
        exit(1);
    }
    if (!file) {
        g_printerr("%s\n", g_option_context_get_help(context, TRUE, NULL));
        exit(1);
    }

    if (strncmp(file[0], "-", 1) == 0) {
        fd = stdin;
    } else {
        fd = fopen(file[0], "r");
    }
    if (fd == NULL) {
        g_printerr("error opening %s\n", argv[1]);
        return 1;
    }
    if (fcntl(fileno(fd), FD_CLOEXEC) < 0) {
        perror("fcntl failed");
        exit(1);
    }
    fseek(fd, 0L, SEEK_END);
    total_size = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    if (total_size > 0)
        g_timeout_add_seconds(1, progress_timer, fd);
    replay = spice_replay_new(fd, MAX_SURFACE_NUM);

    aqueue = g_async_queue_new();
    core = basic_event_loop_init();
    core->channel_event = replay_channel_event;

    server = spice_server_new();
    spice_server_set_image_compression(server, compression);
    spice_server_set_port(server, port);
    spice_server_set_noauth(server);

    g_print("listening on port %d (insecure)\n", port);
    spice_server_init(server, core);

    display_sin.base.sif = &display_sif.base;
    spice_server_add_interface(server, &display_sin.base);

    if (client) {
        start_client(client, &error);
        wait = TRUE;
    }

    if (!wait) {
        started = TRUE;
        fill_queue();
    }

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    end_replay();
    g_async_queue_unref(aqueue);

    /* FIXME: there should be a way to join server threads before:
     * g_main_loop_unref(loop);
     */

    return 0;
}
