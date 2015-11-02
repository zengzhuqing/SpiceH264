#include <glib.h>

#include "spice-session.h"

static void test_session_uri(void)
{
    SpiceSession *s;
    gint i;

    struct {
        gchar *port;
        gchar *tls_port;
        gchar *uri_input;
        gchar *uri_output;
    } tests[] = {
        /* Arguments with empty value */
        { "5900", NULL,
          "spice://localhost?port=5900&tls-port=",
          "spice://localhost?port=5900&" },
        { "5910", NULL,
          "spice://localhost?tls-port=&port=5910",
          "spice://localhost?port=5910&" },
        { NULL, "5920",
          "spice://localhost?tls-port=5920&port=",
          "spice://localhost?tls-port=5920" },
        { NULL, "5930",
          "spice://localhost?port=&tls-port=5930",
          "spice://localhost?tls-port=5930" },
    };

    /* Set URI and check URI, port and tls_port */
    for (i = 0; i < G_N_ELEMENTS(tests); i++) {
        gchar *uri, *port, *tls_port;

        s = spice_session_new();
        g_object_set(s, "uri", tests[i].uri_input, NULL);
        g_object_get(s,
                     "uri", &uri,
                     "port", &port,
                     "tls-port", &tls_port,
                      NULL);
        g_assert_cmpstr(tests[i].uri_output, ==, uri);
        g_assert_cmpstr(tests[i].port, ==, port);
        g_assert_cmpstr(tests[i].tls_port, ==, tls_port);
        g_clear_pointer(&uri, g_free);
        g_clear_pointer(&port, g_free);
        g_clear_pointer(&tls_port, g_free);
        g_object_unref(s);
    }

    /* Set port and tls_port, check URI */
    for (i = 0; i < G_N_ELEMENTS(tests); i++) {
        gchar *uri;

        s = spice_session_new();
        g_object_set(s,
                     "port", tests[i].port,
                     "tls-port", tests[i].tls_port,
                      NULL);
        g_object_get(s, "uri", &uri, NULL);
        g_assert_cmpstr(tests[i].uri_output, ==, uri);
        g_clear_pointer(&uri, g_free);
        g_object_unref(s);
    }
}

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/session/uri", test_session_uri);

    return g_test_run();
}
