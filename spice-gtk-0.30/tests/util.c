#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spice-util-priv.h"

enum {
    DOS2UNIX = 1 << 0,
    UNIX2DOS = 1 << 1,
};

static const struct {
    const gchar *d;
    const gchar *u;
    glong flags;
} dosunix[] = {
    { "", "", DOS2UNIX|UNIX2DOS },
    { "a", "a", DOS2UNIX|UNIX2DOS },
    { "\r\n", "\n", DOS2UNIX|UNIX2DOS },
    { "\r\n\r\n", "\n\n", DOS2UNIX|UNIX2DOS },
    { "a\r\n", "a\n", DOS2UNIX|UNIX2DOS },
    { "a\r\n\r\n", "a\n\n", DOS2UNIX|UNIX2DOS },
    { "\r\n\r\na\r\n\r\n", "\n\na\n\n", DOS2UNIX|UNIX2DOS },
    { "1\r\n\r\na\r\n\r\n2", "1\n\na\n\n2", DOS2UNIX|UNIX2DOS },
    { "\n", "\n", DOS2UNIX },
    { "\n\n", "\n\n", DOS2UNIX },
    { "\r\n", "\r\n", UNIX2DOS },
    { "\r\r\n", "\r\r\n", UNIX2DOS },
    { "é\r\né", "é\né", DOS2UNIX|UNIX2DOS },
    { "\r\né\r\né\r\n", "\né\né\n", DOS2UNIX|UNIX2DOS }
    /* TODO: add some utf8 test cases */
};

static void test_dos2unix(void)
{
    GError *err = NULL;
    gchar *tmp;
    unsigned int i;

    for (i = 0; i < G_N_ELEMENTS(dosunix); i++) {
        if (!(dosunix[i].flags & DOS2UNIX))
            continue;

        tmp = spice_dos2unix(dosunix[i].d, -1, &err);
        g_assert_cmpstr(tmp, ==, dosunix[i].u);
        g_assert_no_error(err);
        g_free(tmp);

        /* including ending \0 */
        tmp = spice_dos2unix(dosunix[i].d, strlen(dosunix[i].d) + 1, &err);
        g_assert_cmpstr(tmp, ==, dosunix[i].u);
        g_assert_no_error(err);
        g_free(tmp);
    }
}

static void test_unix2dos(void)
{
    GError *err = NULL;
    gchar *tmp;
    unsigned int i;

    for (i = 0; i < G_N_ELEMENTS(dosunix); i++) {
        if (!(dosunix[i].flags & UNIX2DOS))
            continue;

        tmp = spice_unix2dos(dosunix[i].u, -1, &err);
        g_assert_cmpstr(tmp, ==, dosunix[i].d);
        g_assert_no_error(err);
        g_free(tmp);

        /* including ending \0 */
        tmp = spice_unix2dos(dosunix[i].u, strlen(dosunix[i].u) + 1, &err);
        g_assert_cmpstr(tmp, ==, dosunix[i].d);
        g_assert_no_error(err);
        g_free(tmp);
    }
}

static const struct {
    unsigned width;
    unsigned height;
    gchar *and;
    gchar *xor;
    gchar *dest;
} mono[] = {
    {
        8, 6,
        "11111111"
        "11111111"
        "11111111"
        "11111111"
        "11111111"
        "11111111"
        ,
        "00000000"
        "00000000"
        "00000100"
        "00000100"
        "00000000"
        "00000000"
        ,
        "0000" "0000" "0000" "0000" "0000" "0000" "0000" "0000"
        "0000" "0000" "0000" "0000" "0001" "0001" "0001" "0000"
        "0000" "0000" "0000" "0000" "0001" "1111" "0001" "0000"
        "0000" "0000" "0000" "0000" "0001" "1111" "0001" "0000"
        "0000" "0000" "0000" "0000" "0001" "0001" "0001" "0000"
        "0000" "0000" "0000" "0000" "0000" "0000" "0000" "0000"
    }
};

static void set_bit(guint8 *d, unsigned bit, unsigned value)
{
    if (value) {
        *d |= (0x80 >> bit);
    } else {
        *d &= ~(0x80 >> bit);
    }
}

static void test_set_bit(void)
{
    struct {
        unsigned len;
        gchar *src;
        gchar *dest;
    } tests[] = {
        {
            4,
            "1111",
            "\xf0",
        },
        {
            16,
            "1111011100110001",
            "\xf7\x31",
        }
    };
    unsigned int i, j, bit;
    guint8 *dest;
    unsigned int bytes;

    for (i = 0 ; i < G_N_ELEMENTS(tests); ++i) {
        bytes = (tests[i].len + 7) / 8;
        dest = g_malloc0(bytes);
        for (j = 0 ; j < tests[i].len;) {
            for (bit = 0 ; bit < 8 && j < tests[i].len; ++bit, ++j) {
                set_bit(&dest[j / 8], bit, tests[i].src[j] == '0' ? 0 : 1);
            }
        }
        for (j = 0 ; j < bytes; ++j) {
            g_assert(dest[j] == (guchar) tests[i].dest[j]);
        }
        g_free(dest);
    }
}

static void test_mono_edge_highlight(void)
{
    unsigned int i;
    int j, bit;
    guint8 *and;
    guint8 *xor;
    guint8 *dest;
    guint8 *dest_correct;
    int size, pixels;

    test_set_bit();

    for (i = 0 ; i < G_N_ELEMENTS(mono); ++i) {
        pixels = mono[i].width * mono[i].height;
        size = (pixels + 7) / 8;
        and = g_malloc0(size);
        xor = g_malloc0(size);
        dest = g_malloc0(pixels * 4);
        dest_correct = g_malloc(pixels * 4);
        for (j = 0 ; j < pixels;) {
            for (bit = 0; bit < 8 && j < pixels; ++bit, ++j) {
                set_bit(&and[j / 8], bit, mono[i].and[j] == '0' ? 0 : 1);
                set_bit(&xor[j / 8], bit, mono[i].xor[j] == '0' ? 0 : 1);
            }
        }
        for (j = 0 ; j < pixels * 4 ; ++j) {
            dest_correct[j] = mono[i].dest[j] == '0' ? 0x00 : 0xff;
        }
        spice_mono_edge_highlight(mono[i].width, mono[i].height, and, xor, dest);
        for (j = 0; j < pixels; ++j) {
            g_assert(dest[j] == dest_correct[j]);
        }
        g_free(and);
        g_free(xor);
        g_free(dest);
        g_free(dest_correct);
    }
}

int main(int argc, char* argv[])
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/util/dos2unix", test_dos2unix);
  g_test_add_func("/util/unix2dos", test_unix2dos);
  g_test_add_func("/util/mono_edge_highlight", test_mono_edge_highlight);

  return g_test_run ();
}
