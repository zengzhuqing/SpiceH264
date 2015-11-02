#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "coroutine.h"

static gpointer co_entry_check_self(gpointer data)
{
    g_assert(data == coroutine_self());
    g_assert(!coroutine_self_is_main());

    return NULL;
}

static gpointer co_entry_42(gpointer data)
{
    g_assert(GPOINTER_TO_INT(data) == 42);
    g_assert(!coroutine_self_is_main());

    return GINT_TO_POINTER(0x42);
}

static void test_coroutine_simple(void)
{
    struct coroutine *self = coroutine_self();
    struct coroutine co = {
        .stack_size = 16 << 20,
        .entry = co_entry_42,
    };
    gpointer result;

    g_assert(coroutine_self_is_main());

    coroutine_init(&co);
    result = coroutine_yieldto(&co, GINT_TO_POINTER(42));
    g_assert_cmpint(GPOINTER_TO_INT(result), ==, 0x42);

#if GLIB_CHECK_VERSION(2,34,0)
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*!to->exited*");
    coroutine_yieldto(&co, GINT_TO_POINTER(42));
    g_test_assert_expected_messages();
#endif

    g_assert(self == coroutine_self());
    g_assert(coroutine_self_is_main());
}

static gpointer co_entry_two(gpointer data)
{
    struct coroutine *self = coroutine_self();
    struct coroutine co = {
        .stack_size = 16 << 20,
        .entry = co_entry_check_self,
    };

    g_assert(!coroutine_self_is_main());
    coroutine_init(&co);
    coroutine_yieldto(&co, &co);

    g_assert(self == coroutine_self());
    return NULL;
}

static void test_coroutine_two(void)
{
    struct coroutine *self = coroutine_self();
    struct coroutine co = {
        .stack_size = 16 << 20,
        .entry = co_entry_two,
    };

    coroutine_init(&co);
    coroutine_yieldto(&co, NULL);

    g_assert(self == coroutine_self());
}

static gpointer co_entry_yield(gpointer data)
{
    gpointer val;

    g_assert(data == NULL);
    val = coroutine_yield(GINT_TO_POINTER(1));
    g_assert_cmpint(GPOINTER_TO_INT(val), ==, 2);

    g_assert(!coroutine_self_is_main());

    val = coroutine_yield(GINT_TO_POINTER(3));
    g_assert_cmpint(GPOINTER_TO_INT(val), ==, 4);

    return NULL;
}

static void test_coroutine_yield(void)
{
    struct coroutine *self = coroutine_self();
    struct coroutine co = {
        .stack_size = 16 << 20,
        .entry = co_entry_yield,
    };
    gpointer val;

    coroutine_init(&co);
    val = coroutine_yieldto(&co, NULL);

    g_assert(self == coroutine_self());
    g_assert_cmpint(GPOINTER_TO_INT(val), ==, 1);

    val = coroutine_yieldto(&co, GINT_TO_POINTER(2));

    g_assert(self == coroutine_self());
    g_assert_cmpint(GPOINTER_TO_INT(val), ==, 3);

    val = coroutine_yieldto(&co, GINT_TO_POINTER(4));

    g_assert(self == coroutine_self());
    g_assert(val == NULL);

#if GLIB_CHECK_VERSION(2,34,0)
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*!to->exited*");
    coroutine_yieldto(&co, GINT_TO_POINTER(42));
    g_test_assert_expected_messages();
#endif
}

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/coroutine/simple", test_coroutine_simple);
    g_test_add_func("/coroutine/two", test_coroutine_two);
    g_test_add_func("/coroutine/yield", test_coroutine_yield);

    return g_test_run ();
}
