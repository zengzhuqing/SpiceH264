/**
 * Test vdagent guest to server messages
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <spice/vd_agent.h>

#include "test_display_base.h"

SpiceCoreInterface *core;
SpiceTimer *ping_timer;

int ping_ms = 100;

#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

void pinger(SPICE_GNUC_UNUSED void *opaque)
{
    // show_channels is not thread safe - fails if disconnections / connections occur
    //show_channels(server);

    core->timer_start(ping_timer, ping_ms);
}

static SpiceBaseInterface base = {
    .type          = SPICE_INTERFACE_CHAR_DEVICE,
    .description   = "test spice virtual channel char device",
    .major_version = SPICE_INTERFACE_CHAR_DEVICE_MAJOR,
    .minor_version = SPICE_INTERFACE_CHAR_DEVICE_MINOR,
};

SpiceCharDeviceInstance vmc_instance = {
    .subtype = "vdagent",
};

int main(void)
{
    Test *test;

    core = basic_event_loop_init();
    test = test_new(core);

    vmc_instance.base.sif = &base;
    spice_server_add_interface(test->server, &vmc_instance.base);

    ping_timer = core->timer_add(pinger, NULL);
    core->timer_start(ping_timer, ping_ms);

    basic_event_loop_mainloop();

    return 0;
}
