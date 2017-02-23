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

static char*
s_readall (const char* filename) {
    FILE *fp = fopen(filename, "rt");
    if (!fp)
        return NULL;

    size_t fsize = 0; 
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *ret = (char*) malloc (fsize * sizeof (char) + 1);
    if (!ret) {
        fclose (fp);
        return NULL;
    }    
    memset ((void*) ret, '\0', fsize * sizeof (char) + 1);

    size_t r = fread((void*) ret, 1, fsize, fp); 
    fclose (fp);
    if (r == fsize)
        return ret; 

    free (ret);
    return NULL;
}

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
        zsys_debug ("Waiting for poller");
         void *which = zpoller_wait (poller, TIMEOUT_MS);
         if (which == NULL) {
             if (zpoller_terminated (poller) || zsys_interrupted) {
                 zsys_info ("Terminating.");
                 break;
             }
         }
        zsys_debug ("Not Waiting for poller");
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

                 char *command = zmsg_popstr (message);
                 if (!command) {
                     zmsg_destroy (&message);
                     zsys_warning ("Empty command.");
                     continue;
                 }
                 if (streq(command, "VERSION")) {
                    zmsg_t *reply = zmsg_new ();
                    char *version = s_readall ("/etc/bios-release.json");
                    if (version == NULL) {
                        zmsg_addstrf (reply, "%s", "ERROR");
                        zmsg_addstrf (reply, "%s", "Version info could not be found");
                        mlm_client_sendto (client, mlm_client_sender (client), "fty-info", NULL, 1000, &reply);
                    }
                    else {
                        zmsg_addstrf (reply, "%s", "VERSION");
                        zmsg_addstrf (reply, "%s", version);
                        mlm_client_sendto (client, mlm_client_sender (client), "fty-info", NULL, 1000, &reply);
                    }
                 }
                 else {
                     zsys_error ("fty-info: Unknown actor command: %s.\n", command);
                 }
                 zstr_free (&command);
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

   static const char* endpoint = "inproc://fty-info-test";

   zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
   zstr_sendx (server, "BIND", endpoint, NULL);
   if (verbose)
       zstr_send (server, "VERBOSE");

   mlm_client_t *ui = mlm_client_new ();
   mlm_client_connect (ui, endpoint, 1000, "UI");

   zactor_t *info_server = zactor_new (fty_info_server, (void*) "fty-info-test");
   //if (verbose)
   //    zstr_send (info_server, "VERBOSE");
   zstr_sendx (info_server, "CONNECT", endpoint, NULL);
   zclock_sleep (1000);

    // Test #1: command VERSION
    {
        zmsg_t *command = zmsg_new ();
        zmsg_addstrf (command, "%s", "VERSION");
        mlm_client_sendto (ui, "fty-info-test", "fty-info", NULL, 1000, &command);

        zmsg_t *recv = mlm_client_recv (ui);

        assert (zmsg_size (recv) == 2);
        char * foo = zmsg_popstr (recv);
        if (streq (foo, "VERSION")) {
            zstr_free (&foo);
            foo = zmsg_popstr (recv);
            zsys_debug ("Received version: \n%s", foo);
        }
        zstr_free (&foo);
        zmsg_destroy (&recv);
    }

    //  @end
    printf ("OK\n");
}
