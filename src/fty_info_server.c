/*  =========================================================================
    fty_info_server - 42ity info server

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
    fty_info_server - 42ity info server
@discuss
@end
*/
#define TIMEOUT_MS 30000   //wait at least 30 seconds

#include "fty_info_classes.h"

//  --------------------------------------------------------------------------
//  Create a new fty_info_server

void
fty_info_server (zsock_t *pipe, void *args)
{
    bool verbose = false;
    mlm_client_t *client = mlm_client_new ();
    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    assert (poller);

    zsock_signal (pipe, 0); 
    zsys_info ("fty-info: Started");

    while (!zsys_interrupted)
    {
         void *which = zpoller_wait (poller, TIMEOUT_MS);
         if (which == NULL) {
             if (zpoller_terminated (poller) || zsys_interrupted) {
                 zsys_info ("Terminating.");
                 break;
             }
         }
         if (which == pipe) {
             if (verbose)
                 zsys_debug ("which == pipe");
             zmsg_t *message = zmsg_recv (pipe);
             if (!message)
                 break;
             
             char *command = zmsg_popstr (message);
             if (!command) {
                 zmsg_destroy (&message);
                 zsys_warning ("Empty command.");
                 continue;
             }
             if (streq(command, "$TERM")) {
                 zsys_info ("Got $TERM");
                 zmsg_destroy (&message);
                 zstr_free (&command);
                 break;
             }
             else
                 if (streq(command, "CONNECT"))
                 {
                     char *endpoint = zmsg_popstr (message);
                     char *name = zmsg_popstr (message);

                     if (endpoint && name) {
                         if (verbose)
                             zsys_debug ("fty-info: CONNECT: %s/%s", endpoint, name);
                         int rv = mlm_client_connect (client, endpoint, 1000, name);
                         if (rv == -1)
                             zsys_error("mlm_client_connect failed\n");
                     }
                     zstr_free (&endpoint);
                     zstr_free (&name);
                 }
                 else
                     if (streq (command, "VERBOSE"))
                     {
                         verbose = true;
                         zsys_debug ("fty-info: VERBOSE=true");
                     }
                     else {
                         zsys_error ("fty-info: Unknown actor command: %s.\n", command);
                     }
             zstr_free (&command);
             zmsg_destroy (&message);
         }
         else
             if (which == mlm_client_msgpipe (client)) {
                 //TODO: implement actor interface
                 zmsg_t *message = mlm_client_recv (client);
                 if (!message)
                    break;
                 zmsg_destroy (&message);
             }
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_info_server_test (bool verbose)
{
    printf (" * fty_info_server: ");

    //  @selftest
    //  @end
    printf ("OK\n");
}
