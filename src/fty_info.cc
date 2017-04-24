/*  =========================================================================
    fty_info - Agent which returns rack controller information

    Copyright (C) 2014 - 2017 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_info - Agent which returns rack controller information
@discuss
@end
*/

#include "fty_info_classes.h"

static int
s_announce_event (zloop_t *loop, int timer_id, void *output)
{
    zstr_send (output, "ANNOUNCE");
    return 0;
}

int main (int argc, char *argv [])
{
    bool verbose = false;
    int announcing = 60;
    int argn;
    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("fty-info [options] ...");
            puts ("  --verbose / -v   verbose test output");
            puts ("  --announce / -a  how often (in seconds) publish information on stream [60]");
            puts ("  --help / -h      this information");
            return 0;
        }
        else if (streq (argv [argn], "--verbose") ||  streq (argv [argn], "-v")) {
            verbose = true;
        }
        else if (streq (argv [argn], "--announce") ||  streq (argv [argn], "-a")) {
            ++argn;
            if (argn < argc) {
                announcing = atoi (argv [argn]);
            }
        }
    }
    if (getenv ("BIOS_LOG_LEVEL") && streq (getenv ("BIOS_LOG_LEVEL"), "LOG_DEBUG"))
                verbose = true;

    zactor_t *server = zactor_new (fty_info_server, (void*) FTY_INFO_AGENT);

    //  Insert main code here
    if (verbose) {
        zstr_sendx (server, "VERBOSE", NULL);
        zsys_info ("fty_info - Agent which returns rack controller information");
    }

    zstr_sendx (server, "CONNECT", "ipc://@/malamute", FTY_INFO_AGENT, NULL);
    zstr_sendx (server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zstr_sendx (server, "PRODUCER", "ANNOUNCE", NULL);

    zloop_t *announce = zloop_new();
    zloop_timer (announce, announcing * 1000, 0, s_announce_event, server);
    zloop_start (announce);

    zloop_destroy (&announce);
    zactor_destroy (&server);
    return 0;
}
