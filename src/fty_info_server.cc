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
static const char* RELEASE_DETAILS = "/etc/release-details.json";

#define FTY_INFO_TEST "info-test" 

#include <string>
#include <unistd.h>
#include <bits/local_lim.h>
#include <tntdb/connect.h>
#include <tntdb/result.h>
#include <tntdb/error.h>
#include <cxxtools/jsondeserializer.h>
#include <istream>
#include <fstream>

#include "fty_info_classes.h"

struct _fty_info_t {
    zhash_t *infos;
    char *uuid;
    char *hostname;
    char *name;
    char *product_name;
    char *location;
    char *version;
    char *rest_root;
    char *rest_port;
};

const char *TST_UUID="ce7c523e-08bf-11e7-af17-080027d52c4f";
const char *TST_HOSTNAME="localhost";
const char *TST_NAME="ipc-001";
const char *TST_PRODUCT_NAME="IPC3000";
const char *TST_LOCATION="Rack1";
const char *TST_VERSION="1.0.0";

std::vector<std::string> rest_roots { "/api/v1/" };
std::vector<std::string> rest_ports { "8000", "80", "443" };

static int
s_get_product_name
    (std::istream &f,
     std::string &product_name)
{
    try {
        cxxtools::SerializationInfo si;
        std::string json_string (std::istreambuf_iterator<char>(f), {});
        std::stringstream s(json_string);
        cxxtools::JsonDeserializer json(s);
        json.deserialize (si);
        //if (si.memberCount == 0)
        //throw std::runtime_error ("Document /etc/release-details.json is empty");
        si.getMember("release-details").getMember("hardware-catalog-number") >>= product_name;
        return 0;
    }
    catch (const std::exception& e) {
        zsys_error ("Error while parsing JSON: %s", e.what ());
        return -1;
    }
}

static int
s_select_rack_controller_parent
    (tntdb::Connection &conn,
     char *name,
     std::string &parent_name)
{
    try {
        tntdb::Statement st = conn.prepareCached(
        " SELECT "
        "   t1.parent_name "
        " FROM "
        "   v_bios_asset_element t1"
        " WHERE "
        "   t1.name = :name "
        );
        tntdb::Result result = st.set ("name", name).select ();
        if (result.size () > 1) {
            zsys_error ("asset '%s' has more than one parent, DB is messed up");
            return -2;
        }
        for (auto &row: result) {
            row ["parent_name"].get (parent_name);
            zsys_debug ("Found parent '%s'", parent_name.c_str ());
        }
        return 0;
    }
    catch (const std::exception& e) {
        zsys_error ("Error: %s", e.what ());
        return -1;
    }
}

//TODO: change after we add display name
static int
s_select_rack_controller_name
    (tntdb::Connection &conn,
     std::string &name)
{
    try {
        tntdb::Statement st = conn.prepareCached(
        " SELECT "
        "   t1.name "
        " FROM "
        "   (v_bios_asset_element t1 INNER JOIN v_bios_asset_device_type t2 ON (t1.id_subtype = t2.id))"
        " WHERE "
        "   t2.name = 'rack controller'"
        );
        tntdb::Result result = st.select ();
        if (result.size () > 1) {
            zsys_error ("fty-info found more than one RC, not sure what to do");
            return -2;
        }

        for (auto &row: result) {
            row ["name"].get (name);
            zsys_debug ("Found RC '%s'", name.c_str ());
        }
        return 0;
    }
    catch (const std::exception& e) {
        zsys_error ("Error: %s", e.what ());
        return -1;
    }
}
fty_info_t*
fty_info_test_new (void)
{
    fty_info_t *self = (fty_info_t *) malloc (sizeof (fty_info_t));
    self->infos = zhash_new();
    self->uuid = strdup (TST_UUID);
    self->hostname = strdup (TST_HOSTNAME);
    self->name = strdup (TST_NAME);
    self->product_name = strdup (TST_PRODUCT_NAME);
    self->location = strdup (TST_LOCATION);
    self->version = strdup (TST_VERSION);
    self->rest_root = strdup (rest_roots.at(0).c_str());
    self->rest_port = strdup (rest_ports.at(0).c_str());
    return self;
}

fty_info_t*
fty_info_new (void)
{
    fty_info_t *self = (fty_info_t *) malloc (sizeof (fty_info_t));
    tntdb::Connection conn;
    std::string url = std::string("mysql:db=box_utf8;user=") +
    ((getenv("DB_USER")   == NULL) ? "root" : getenv("DB_USER")) +
    ((getenv("DB_PASSWD") == NULL) ? ""     :   
     std::string(";password=") + getenv("DB_PASSWD"));

    try {
        conn = tntdb::connectCached (url);
    }
    catch ( const std::exception &e) {
        zsys_error ("DB: cannot connect, %s", e.what());
    }    
    self->infos = zhash_new();
    
    // set uuid - generated by `uuid -v5 <name_space_based_on_uuid> <(Concatenation of UTF8 encoded: vendor + model + serial)>`
    // TODO: /etc/release-details.json doesn't contain vendor, and serial number is empty 
    self->uuid = strdup ("");
    
    
    // set hostname
    char *hostname = (char *) malloc (HOST_NAME_MAX+1);
    int rv = gethostname (hostname, HOST_NAME_MAX+1);
    if (rv == -1) {
        zsys_warning ("fty_info could not be fully initialized (error while getting the hostname)");
        self->hostname = strdup("");
    }
    else {
        self->hostname = strdup (hostname);
    }
    zstr_free (&hostname);
    zsys_debug ("hostname = '%s'", self->hostname);
    
    //set name
    std::string name;
    rv = s_select_rack_controller_name (conn, name);
    if (rv < 0) {
        zsys_warning ("fty_info could not be fully initialized (error while getting the name)");
        self->name = strdup ("");
    }
    else {
        self->name = strdup (name.c_str ());
    }
    zsys_debug ("name set to the value '%s'", self-> name);
    
    // set product name - "hardware-catalog-number" from /etc/release-details.json (first part?)
    std::ifstream f(RELEASE_DETAILS);
    std::string product_name;
    rv = s_get_product_name (f, product_name);
    if (rv < 0) {
        zsys_warning ("fty_info could not be fully initialized (error while getting the product name)");
        self->product_name = strdup ("");
    }
    else {
        self->product_name = strdup (product_name.c_str ());
    }
    zsys_debug ("Product name set to the value '%s'", self->product_name);
    
    // set location (parent)
    std::string parent;
    rv = s_select_rack_controller_parent (conn, self->name, parent);
    if (rv < 0) {
        zsys_warning ("fty_info could not be fully initialized (error while getting the location)");
        self->location = strdup ("");
    }
    else {
        self->location = strdup (parent.c_str ());
    }
    zsys_debug ("location set to the value '%s'", self->location);
    
    // TODO: set version
    self->version = strdup ("");
    // TODO: set rest_root - what if we can find more than one?
    self->rest_root = strdup ("");
    // TODO: set rest_port - what if we can find more than one?
    self->rest_port = 0;

    conn.clearStatementCache ();
    conn.close ();
    return self;
}

void
fty_info_destroy (fty_info_t ** self_ptr)
{
    if (!self_ptr)
        return;
    if (*self_ptr) {
        fty_info_t *self = *self_ptr;
        // Free class properties here
        zhash_destroy(&self->infos);
        zstr_free (&self->uuid);
        zstr_free (&self->hostname);
        zstr_free (&self->name);
        zstr_free (&self->product_name);
        zstr_free (&self->location);
        zstr_free (&self->version);
        zstr_free (&self->rest_root);
        // Free object itself
        free (self);
        *self_ptr = NULL;
    }

}

//  --------------------------------------------------------------------------
//  Create a new fty_info_server

void
fty_info_server (zsock_t *pipe, void *args)
{
    char *name = (char *)args;
    if (!name) {
        zsys_error ("Adress for fty-info actor is NULL");
        return;
    }
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

                     if (endpoint) {
                         zsys_debug ("fty-info: CONNECT: %s/%s", endpoint, name);
                         int rv = mlm_client_connect (client, endpoint, 1000, name);
                         if (rv == -1)
                             zsys_error("mlm_client_connect failed\n");
                     }
                     zstr_free (&endpoint);
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
                    continue;

                 char *command = zmsg_popstr (message);
                 if (!command) {
                    zmsg_destroy (&message);
                    zsys_warning ("Empty command.");
                    continue;
                 }
                 if (!streq(command, "INFO") && !streq(command, "INFO-TEST")) {
                    zsys_warning ("fty-info: Received unexpected command '%s'", command);
                    zmsg_t *reply = zmsg_new ();
                    zmsg_addstr(reply, "ERROR");
                    mlm_client_sendto (client, mlm_client_sender (client), "info", NULL, 1000, &reply);
                    zstr_free (&command);
                    zmsg_destroy (&message);
                    continue;
                     
                 } else {
                    zmsg_t *reply = zmsg_new ();
                    char *zuuid = zmsg_popstr (message);
                    fty_info_t *self;
                    if (streq(command, "INFO")) {
                        self = fty_info_new ();
                    }
                    if (streq(command, "INFO-TEST")) {
                        self = fty_info_test_new ();
 
                    }
                    //prepare replied msg content
                    zmsg_addstrf (reply, "%s", zuuid);
                    zhash_insert(self->infos, INFO_UUID, self->uuid);
                    zhash_insert(self->infos, INFO_HOSTNAME, self->hostname);
                    zhash_insert(self->infos, INFO_NAME, self->name);
                    zhash_insert(self->infos, INFO_PRODUCT_NAME, self->product_name);
                    zhash_insert(self->infos, INFO_LOCATION, self->location);
                    zhash_insert(self->infos, INFO_VERSION, self->version);
                    zhash_insert(self->infos, INFO_REST_ROOT, self->rest_root);
                    zhash_insert(self->infos, INFO_REST_PORT, self->rest_port);
                    zframe_t * frame_infos = zhash_pack(self->infos);
                    zmsg_append (reply, &frame_infos);
               
                    mlm_client_sendto (client, mlm_client_sender (client), "info", NULL, 1000, &reply);
                    zstr_free (&zuuid);
                    fty_info_destroy (&self);
                }
                zstr_free (&command);
                zmsg_destroy (&message);
             }
    }
    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
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
   zuuid_t *zuuid = zuuid_new ();
   
   zactor_t *info_server = zactor_new (fty_info_server, (void*) "fty-info-test");
   if (verbose)
       zstr_send (info_server, "VERBOSE");
   zstr_sendx (info_server, "CONNECT", endpoint, NULL);
   zclock_sleep (1000);

    // Test #1: command INFO
    {
    zmsg_t *command = zmsg_new ();
    zmsg_addstrf (command, "%s", "INFO-TEST");
    zmsg_addstrf (command, "%s", zuuid_str_canonical (zuuid));
    mlm_client_sendto (ui, "fty-info-test", FTY_INFO_AGENT, NULL, 1000, &command);

    zmsg_t *recv = mlm_client_recv (ui);

    assert (zmsg_size (recv) == 2);
    zsys_debug ("fty-info: zmsg_size = %d",zmsg_size (recv));
    char *zuuid_reply = zmsg_popstr (recv);
    assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
    
    zframe_t *frame_infos = zmsg_next (recv);
    zhash_t *infos = zhash_unpack(frame_infos); 
    
    char * uuid = (char *) zhash_lookup (infos, INFO_UUID);
    assert(uuid && streq (uuid,TST_UUID));
    zsys_debug ("fty-info: uuid = '%s'", uuid);
    char * hostname = (char *) zhash_lookup (infos, INFO_HOSTNAME);
    assert(hostname && streq (hostname, TST_HOSTNAME));
    zsys_debug ("fty-info: hostname = '%s'", hostname);
    char * name = (char *) zhash_lookup (infos, INFO_NAME);
    assert(name && streq (name, TST_NAME));
    zsys_debug ("fty-info: name = '%s'", name);
    char * product_name = (char *) zhash_lookup (infos, INFO_PRODUCT_NAME);
    assert(product_name && streq (product_name, TST_PRODUCT_NAME));
    zsys_debug ("fty-info: product_name = '%s'", product_name);
    char * location = (char *) zhash_lookup (infos, INFO_LOCATION);
    assert(location && streq (location, TST_LOCATION));
    zsys_debug ("fty-info: location = '%s'", location);
    char * version = (char *) zhash_lookup (infos, INFO_VERSION);
    assert(version && streq (version, TST_VERSION));
    zsys_debug ("fty-info: version = '%s'", version);
    char * rest_root = (char *) zhash_lookup (infos, INFO_REST_ROOT);
    assert(rest_root && streq (rest_root, rest_roots.at(0).c_str()));
    zsys_debug ("fty-info: rest_root = '%s'", rest_root);
    char * rest_port = (char *) zhash_lookup (infos, INFO_REST_PORT);
    assert(rest_port && streq (rest_port, rest_ports.at(0).c_str()));
    zsys_debug ("fty-info: rest_port = '%s'", rest_port);
    zhash_destroy(&infos);
 
    
    zmsg_destroy (&recv);
    printf ("zuuid = '%s'\n", zuuid_str_canonical (zuuid));
    zuuid_destroy (&zuuid);
    }

    //  @end
    printf ("OK\n");
    zactor_destroy (&info_server);
    mlm_client_destroy (&ui);
    zactor_destroy (&server);
}
