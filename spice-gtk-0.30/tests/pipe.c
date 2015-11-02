#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "giopipe.h"

typedef struct _Fixture {
    GIOStream *p1;
    GIOStream *p2;

    GInputStream *ip1;
    GOutputStream *op1;
    GInputStream *ip2;
    GOutputStream *op2;

    gchar buf[16];
    gchar *data;
    guint16 data_len;
    guint16 read_size;
    guint16 total_read;

    GList *sources;

    GMainLoop *loop;
    GCancellable *cancellable;
    guint timeout;
} Fixture;

static gboolean
stop_loop (gpointer data)
{
    GMainLoop *loop = data;

    g_main_loop_quit (loop);
    g_assert_not_reached();

    return G_SOURCE_REMOVE;
}

static void
fixture_set_up(Fixture *fixture,
               gconstpointer user_data)
{
    int i;

    spice_make_pipe(&fixture->p1, &fixture->p2);
    g_assert_true(G_IS_IO_STREAM(fixture->p1));
    g_assert_true(G_IS_IO_STREAM(fixture->p2));

    fixture->op1 = g_io_stream_get_output_stream(fixture->p1);
    g_assert_true(G_IS_OUTPUT_STREAM(fixture->op1));
    fixture->ip1 = g_io_stream_get_input_stream(fixture->p1);
    g_assert_true(G_IS_INPUT_STREAM(fixture->ip1));
    fixture->op2 = g_io_stream_get_output_stream(fixture->p2);
    g_assert_true(G_IS_OUTPUT_STREAM(fixture->op2));
    fixture->ip2 = g_io_stream_get_input_stream(fixture->p2);
    g_assert_true(G_IS_INPUT_STREAM(fixture->ip2));

    for (i = 0; i < sizeof(fixture->buf); i++) {
        fixture->buf[i] = 0x42 + i;
    }

    fixture->sources = NULL;
    fixture->cancellable = g_cancellable_new();
    fixture->loop = g_main_loop_new (NULL, FALSE);
    fixture->timeout = g_timeout_add (1000, stop_loop, fixture->loop);
}

static void
fixture_tear_down(Fixture *fixture,
                  gconstpointer user_data)
{
    g_clear_object(&fixture->p1);
    g_clear_object(&fixture->p2);

    if (fixture->sources)
        g_list_free_full(fixture->sources, (GDestroyNotify) g_source_unref);

    g_clear_pointer(&fixture->data, g_free);
    g_clear_object(&fixture->cancellable);
    g_source_remove(fixture->timeout);
    g_main_loop_unref(fixture->loop);
}

static void
test_pipe_readblock(Fixture *f, gconstpointer user_data)
{
    GError *error = NULL;
    gssize size;

    size = g_input_stream_read(f->ip2, f->buf, 1,
                               f->cancellable, &error);

    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);

    g_clear_error(&error);
}

static void
test_pipe_writeblock(Fixture *f, gconstpointer user_data)
{
    GError *error = NULL;
    gssize size;

    size = g_output_stream_write(f->op1, "", 1,
                                 f->cancellable, &error);

    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);

    g_clear_error(&error);
}

static void
write_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GMainLoop *loop = user_data;
    gssize nbytes;

    nbytes = g_output_stream_write_finish(G_OUTPUT_STREAM(source), result, &error);

    g_assert_no_error(error);
    g_assert_cmpint(nbytes, >, 0);
    g_clear_error(&error);

    g_main_loop_quit (loop);
}

static void
read_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    gssize nbytes, expected = GPOINTER_TO_INT(user_data);

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);

    g_assert_cmpint(nbytes, ==, expected);
    g_assert_no_error(error);
    g_clear_error(&error);
}

static void
test_pipe_writeread(Fixture *f, gconstpointer user_data)
{
    g_output_stream_write_async(f->op1, "", 1, G_PRIORITY_DEFAULT,
                                f->cancellable, write_cb, f->loop);
    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, read_cb, GINT_TO_POINTER(1));

    g_main_loop_run (f->loop);

    g_output_stream_write_async(f->op1, "", 1, G_PRIORITY_DEFAULT,
                                f->cancellable, write_cb, f->loop);
    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, read_cb, GINT_TO_POINTER(1));

    g_main_loop_run (f->loop);
}

static void
test_pipe_readwrite(Fixture *f, gconstpointer user_data)
{
    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, read_cb, GINT_TO_POINTER(1));
    g_output_stream_write_async(f->op1, "", 1, G_PRIORITY_DEFAULT,
                                f->cancellable, write_cb, f->loop);

    g_main_loop_run (f->loop);
}

static void
test_pipe_write16read8(Fixture *f, gconstpointer user_data)
{
    g_output_stream_write_async(f->op1, "0123456789abcdef", 16, G_PRIORITY_DEFAULT,
                                f->cancellable, write_cb, f->loop);
    g_input_stream_read_async(f->ip2, f->buf, 8, G_PRIORITY_DEFAULT,
                              f->cancellable, read_cb, GINT_TO_POINTER(8));

    g_main_loop_run (f->loop);

    /* check next read would block */
    test_pipe_readblock(f, user_data);
}

static void
test_pipe_write8read16(Fixture *f, gconstpointer user_data)
{
    g_output_stream_write_async(f->op1, "01234567", 8, G_PRIORITY_DEFAULT,
                                f->cancellable, write_cb, f->loop);
    g_input_stream_read_async(f->ip2, f->buf, 16, G_PRIORITY_DEFAULT,
                              f->cancellable, read_cb, GINT_TO_POINTER(8));

    g_main_loop_run (f->loop);

    /* check next read would block */
    test_pipe_writeblock(f, user_data);
}

static void
readclose_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    gssize nbytes;
    GMainLoop *loop = user_data;

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);

    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CLOSED);
    g_clear_error(&error);

    g_main_loop_quit (loop);
}

static void
test_pipe_readclosestream(Fixture *f, gconstpointer user_data)
{
    GError *error = NULL;

    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, readclose_cb, f->loop);
    g_io_stream_close(f->p1, f->cancellable, &error);

    g_main_loop_run (f->loop);
}

static void
test_pipe_readclose(Fixture *f, gconstpointer user_data)
{
    GError *error = NULL;

    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, readclose_cb, f->loop);
    g_output_stream_close(f->op1, f->cancellable, &error);

    g_main_loop_run (f->loop);
}

static void
readcancel_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    gssize nbytes;
    GMainLoop *loop = user_data;

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);

    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CLOSED);
    g_clear_error(&error);

    g_main_loop_quit (loop);
}

static void
test_pipe_readcancel(Fixture *f, gconstpointer user_data)
{
    GError *error = NULL;

    g_input_stream_read_async(f->ip2, f->buf, 1, G_PRIORITY_DEFAULT,
                              f->cancellable, readcancel_cb, f->loop);
    g_output_stream_close(f->op1, f->cancellable, &error);

    g_main_loop_run (f->loop);
}

static gchar *
get_test_data(gint n)
{
    GString *s = g_string_sized_new(n);
    const gchar *data = "01234567abcdefgh";
    gint i, q;

    q = n / 16;
    for (i = 0; i < q; i++)
        s = g_string_append(s, data);

    s = g_string_append_len(s, data, (n % 16));
    return g_string_free(s, FALSE);
}

static void
write_all_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Fixture *f = user_data;
    GError *error = NULL;
    gsize nbytes;

    g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result, &nbytes, &error);
    g_assert_no_error(error);
    g_assert_cmpint(nbytes, ==, f->data_len);
    g_clear_error(&error);

    g_main_loop_quit (f->loop);
}

static void
read_chunk_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Fixture *f = user_data;
    GError *error = NULL;
    gssize nbytes;
    gboolean data_match;

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);
    g_assert_no_error(error);
    g_assert_cmpint(nbytes, >, 0);
    data_match = (g_ascii_strncasecmp(f->data + f->total_read, f->buf, nbytes) == 0);
    g_assert_true(data_match);

    f->total_read += nbytes;
    if (f->total_read != f->data_len) {
        g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                                  f->cancellable, read_chunk_cb, f);
    }
}

static void
test_pipe_write_all_64_read_chunks_16(Fixture *f, gconstpointer user_data)
{
    f->data_len = 64;
    f->data = get_test_data(f->data_len);
    f->read_size = 16;
    f->total_read = 0;

    g_output_stream_write_all_async(f->op1, f->data, f->data_len, G_PRIORITY_DEFAULT,
                                    f->cancellable, write_all_cb, f);
    g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                              f->cancellable, read_chunk_cb, f);
    g_main_loop_run (f->loop);
}

static void
read_chunk_cb_and_try_write(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Fixture *f = user_data;
    GError *error = NULL;
    gssize nbytes;
    gboolean data_match;

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);
    g_assert_no_error(error);
    g_assert_cmpint(nbytes, >, 0);
    data_match = (g_ascii_strncasecmp(f->data + f->total_read, f->buf, nbytes) == 0);
    g_assert_true(data_match);

    f->total_read += nbytes;
    if (f->total_read != f->data_len) {
        /* try write before reading another chunk */
        g_output_stream_write(f->op1, "", 1, f->cancellable, &error);
        g_assert_error(error, G_IO_ERROR, G_IO_ERROR_PENDING);
        g_clear_error(&error);

        g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                                  f->cancellable, read_chunk_cb_and_try_write, f);
    }
}

static void
test_pipe_concurrent_write(Fixture *f, gconstpointer user_data)
{
    f->data_len = 64;
    f->data = get_test_data(f->data_len);
    f->read_size = 16;
    f->total_read = 0;

    g_output_stream_write_all_async(f->op1, f->data, f->data_len, G_PRIORITY_DEFAULT,
                                    f->cancellable, write_all_cb, f);
    g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                              f->cancellable, read_chunk_cb_and_try_write, f);
    g_main_loop_run (f->loop);
}

static void
write_all_cb_zombie_check(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Fixture *f = user_data;
    GError *error = NULL;
    gsize nbytes;
    GList *it;

    g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result, &nbytes, &error);
    g_assert_no_error(error);
    g_assert_cmpint(nbytes, ==, f->data_len);
    g_clear_error(&error);

    for (it = f->sources; it != NULL; it = it->next) {
        GSource *s = it->data;
        g_assert_true (g_source_is_destroyed (s));
    }

    g_main_loop_quit (f->loop);
}

static gboolean
source_cb (gpointer user_data)
{
    return G_SOURCE_REMOVE;
}

#define NUM_OF_DUMMY_GSOURCE 1000

static void
read_chunk_cb_and_do_zombie(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Fixture *f = user_data;
    GError *error = NULL;
    gssize nbytes;
    gboolean data_match, try_zombie;
    gint i;

    nbytes = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);
    g_assert_no_error(error);
    g_assert_cmpint(nbytes, >, 0);
    data_match = (g_ascii_strncasecmp(f->data + f->total_read, f->buf, nbytes) == 0);
    g_assert_true(data_match);

    /* Simulate more Pollable GSources created to read from Pipe; This should
     * not fail but giopipe does not allow concurrent read/write which means
     * that only the *last* GSource created will be the one that does the actual
     * read; The other GSources that are still active should be dispatched.
     * (In this test, only the real GSource created in g_input_stream_read_async
     * will read the data) */

    /* Create GSources in all iterations besides the last one, simply because
     * it is convenient! The execution of the last interaction should give enough
     * time for for all dummy GSources being detached. */
    f->total_read += nbytes;
    try_zombie = (f->total_read + f->read_size < f->data_len);

    if (try_zombie) {
        for (i = 0; i < NUM_OF_DUMMY_GSOURCE/2; i++) {
            GSource *s = g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(f->ip2), NULL);
            g_source_set_callback(s, source_cb, NULL, NULL);
            g_source_attach(s, NULL);
            f->sources = g_list_prepend(f->sources, s);
        }
    }

    if (f->total_read != f->data_len)
        g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                                  f->cancellable, read_chunk_cb_and_do_zombie, f);

    if (try_zombie) {
        for (i = 0; i < NUM_OF_DUMMY_GSOURCE/2; i++) {
            GSource *s = g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(f->ip2), NULL);
            g_source_set_callback(s, source_cb, NULL, NULL);
            g_source_attach(s, NULL);
            f->sources = g_list_prepend(f->sources, s);
        }
    }
}

static void
test_pipe_zombie_sources(Fixture *f, gconstpointer user_data)
{
    gint i;
    f->data_len = 64;
    f->data = get_test_data(f->data_len);
    f->read_size = 16;
    f->total_read = 0;

    g_output_stream_write_all_async(f->op1, f->data, f->data_len, G_PRIORITY_DEFAULT,
                                    f->cancellable, write_all_cb_zombie_check, f);
    g_input_stream_read_async(f->ip2, f->buf, f->read_size, G_PRIORITY_DEFAULT,
                              f->cancellable, read_chunk_cb_and_do_zombie, f);
    g_main_loop_run (f->loop);
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/pipe/readblock", Fixture, NULL,
               fixture_set_up, test_pipe_readblock,
               fixture_tear_down);

    g_test_add("/pipe/writeblock", Fixture, NULL,
               fixture_set_up, test_pipe_writeblock,
               fixture_tear_down);

    g_test_add("/pipe/writeread", Fixture, NULL,
               fixture_set_up, test_pipe_writeread,
               fixture_tear_down);

    g_test_add("/pipe/readwrite", Fixture, NULL,
               fixture_set_up, test_pipe_readwrite,
               fixture_tear_down);

    g_test_add("/pipe/write16read8", Fixture, NULL,
               fixture_set_up, test_pipe_write16read8,
               fixture_tear_down);

    g_test_add("/pipe/write8read16", Fixture, NULL,
               fixture_set_up, test_pipe_write8read16,
               fixture_tear_down);

    g_test_add("/pipe/write-all64-read-chunks16", Fixture, NULL,
               fixture_set_up, test_pipe_write_all_64_read_chunks_16,
               fixture_tear_down);

    g_test_add("/pipe/concurrent-write", Fixture, NULL,
               fixture_set_up, test_pipe_concurrent_write,
               fixture_tear_down);

    g_test_add("/pipe/zombie-sources", Fixture, NULL,
               fixture_set_up, test_pipe_zombie_sources,
               fixture_tear_down);

    g_test_add("/pipe/readclosestream", Fixture, NULL,
               fixture_set_up, test_pipe_readclosestream,
               fixture_tear_down);

    g_test_add("/pipe/readclose", Fixture, NULL,
               fixture_set_up, test_pipe_readclose,
               fixture_tear_down);

    g_test_add("/pipe/readcancel", Fixture, NULL,
               fixture_set_up, test_pipe_readcancel,
               fixture_tear_down);

    return g_test_run();
}
