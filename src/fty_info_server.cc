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

//test value for INFO-TEST command reply
#define TST_UUID     "ce7c523e-08bf-11e7-af17-080027d52c4f"
#define TST_HOSTNAME "localhost"
#define TST_NAME     "ipc-001"
#define TST_MODEL    "IPC3000"
#define TST_VENDOR   "Eaton"
#define TST_SERIAL   "LA71026006"
#define TST_LOCATION "Rack1"
#define TST_VERSION  "1.0.0"
#define TST_PATH     "/api/v1"
#define TST_PORT     "80"


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

static std::string url =
    std::string("mysql:db=box_utf8;user=") +
    ((getenv("DB_USER")   == NULL) ? "root" : getenv("DB_USER")) +
    ((getenv("DB_PASSWD") == NULL) ? ""     :
    std::string(";password=") + getenv("DB_PASSWD"));

struct _fty_info_t {
    zhash_t *infos;
    char *uuid;
    char *hostname;
    char *name;
    char *model;
    char *vendor;
    char *serial;
    char *location;
    char *version;
    char *rest_path;
    char *rest_port;
};


static cxxtools::SerializationInfo*
s_load_release_details()
{
    cxxtools::SerializationInfo *si = new cxxtools::SerializationInfo();
    try {
        std::ifstream f(RELEASE_DETAILS);
        std::string json_string (std::istreambuf_iterator<char>(f), {});
        std::stringstream s(json_string);
        cxxtools::JsonDeserializer json(s);
        json.deserialize (*si);
        zsys_info("fty-info:load %s OK",RELEASE_DETAILS);
    }
    catch (const std::exception& e) {
        zsys_error ("Error while parsing JSON: %s", e.what ());
    }
    return si;
}


static char*
s_get_release_details
    (cxxtools::SerializationInfo *si,
     const char *key,
     const char * dfl)
{
    std::string value = dfl;
    try {
        si->getMember("release-details").getMember(key) >>= value;
    }
    catch (const std::exception& e) {
        zsys_error ("Error while getting %s in JSON: %s", key, e.what ());
    }
    return strdup(value.c_str());
}

static char*
s_select_rack_controller_parent
    (tntdb::Connection &conn,
     char *name,
     const char * dfl)
{
    std::string parent_name = dfl;
    if (!conn)return strdup(dfl);
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
            zsys_warning ("asset '%s' has more than one parent, DB is messed up",name);
        }
        for (auto &row: result) {
            row ["parent_name"].get (parent_name);
        }
    }
    catch (const std::exception& e) {
        zsys_error ("Error: %s", e.what ());
    }
    return strdup(parent_name.c_str());
    
}

//TODO: change after we add display name

static char*
s_select_rack_controller_name
    (tntdb::Connection &conn,
     const char * dfl)
{
    std::string name;
    if (!conn)return strdup(dfl);
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
            zsys_warning ("fty-info found more than one RC, not sure what to do");
        }
        for (auto &row: result) {
            row ["name"].get (name);
        }
    }
    catch (const std::exception& e) {
        zsys_error ("Error: %s", e.what ());
    }
    return strdup(name.c_str());
}
fty_info_t*
fty_info_test_new (void)
{
    fty_info_t *self = (fty_info_t *) malloc (sizeof (fty_info_t));
    self->infos     = zhash_new();
    self->uuid      = strdup (TST_UUID);
    self->hostname  = strdup (TST_HOSTNAME);
    self->name      = strdup (TST_NAME);
    self->model     = strdup (TST_MODEL);
    self->vendor    = strdup (TST_VENDOR);
    self->serial    = strdup (TST_SERIAL);
    self->location  = strdup (TST_LOCATION);
    self->version   = strdup (TST_VERSION);
    self->rest_path = strdup (TST_PATH);
    self->rest_port = strdup (TST_PORT);
    return self;
}

fty_info_t*
fty_info_new (void)
{
    fty_info_t *self = (fty_info_t *) malloc (sizeof (fty_info_t));
    self->infos = zhash_new();
    
    // set hostname
    char *hostname = (char *) malloc (HOST_NAME_MAX+1);
    int rv = gethostname (hostname, HOST_NAME_MAX+1);
    if (rv == -1) {
        zsys_warning ("fty_info could not be fully initialized (error while getting the hostname)");
        self->hostname = strdup("locahost");
    }
    else {
        self->hostname = strdup (hostname);
    }
    zstr_free (&hostname);
    zsys_info ("fty-info:hostname  = '%s'", self->hostname);
            
    tntdb::Connection conn;

    try {
        conn = tntdb::connectCached (url);
        conn.ping();
        zsys_info("fty-info:DB connection OK",RELEASE_DETAILS);
    }
    catch ( const std::exception &e) {
        zsys_error ("DB: cannot connect, %s", e.what());
        conn = tntdb::Connection();
    } 
    //set name
    self->name = s_select_rack_controller_name (conn, "NA");
    zsys_info ("fty-info:name      = '%s'", self-> name);
    
    //set location (parent)
    self->location  = s_select_rack_controller_parent (conn, self->name, "NA");
    zsys_info ("fty-info:location  = '%s'", self->location);

    //set uuid, vendor, model from /etc/release-details.json 
    cxxtools::SerializationInfo *si = nullptr;
    si = s_load_release_details();
    self->uuid   = s_get_release_details (si, "uuid", "00000000-0000-0000-0000-000000000000");
    self->vendor = s_get_release_details (si, "hardware-vendor", "NA");
    self->serial = s_get_release_details (si, "hardware-serial-number", "NA");
    self->model  = s_get_release_details (si, "hardware-catalog-number", "NA");
    zsys_info ("fty-info:uuid      = '%s'", self->uuid);
    zsys_info ("fty-info:vendor    = '%s'", self->vendor);
    zsys_info ("fty-info:serial    = '%s'", self->serial);
    zsys_info ("fty-info:model     = '%s'", self->model);
    
    
    // TODO: set version
    self->version   = strdup ("NotImplemented");
    // use default
    self->rest_path = strdup ("/api/v1");
    // use default
    self->rest_port = strdup ("443");
    
    zsys_info ("fty-info:version   = '%s'", self->version);
    zsys_info ("fty-info:rest_path = '%s'", self->rest_path);
    zsys_info ("fty-info:rest_port = '%s'", self->rest_port);

    tntdb::dropCached();
     if(si)
        delete si;
    
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
        zstr_free (&self->model);
        zstr_free (&self->vendor);
        zstr_free (&self->serial);
        zstr_free (&self->location);
        zstr_free (&self->version);
        zstr_free (&self->rest_port);
        zstr_free (&self->rest_path);
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
        zsys_error ("Address for fty-info actor is NULL");
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
                    zsys_warning ("Empty subject.");
                    continue;
                 }
                 //we assume all request command are MAILBOX DELIVER, and subject="info"
                 if (!streq(command, "INFO") && !streq(command, "INFO-TEST")) {
                    zsys_warning ("fty-info: Received unexpected command '%s'", command);
                    zmsg_t *reply = zmsg_new ();
                    zmsg_addstr(reply, "ERROR");
                    mlm_client_sendto (client, mlm_client_sender (client), "info", NULL, 1000, &reply);
                    zstr_free (&command);
                    zmsg_destroy (&message);
                    continue;
                     
                 } else {
                    zsys_debug ("fty-info:do '%s'", command);
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
                    zhash_insert(self->infos, INFO_VENDOR, self->vendor);
                    zhash_insert(self->infos, INFO_MODEL, self->model);
                    zhash_insert(self->infos, INFO_SERIAL, self->serial);
                    zhash_insert(self->infos, INFO_LOCATION, self->location);
                    zhash_insert(self->infos, INFO_VERSION, self->version);
                    zhash_insert(self->infos, INFO_REST_PATH, self->rest_path);
                    zhash_insert(self->infos, INFO_REST_PORT, self->rest_port);
                    
                    zframe_t * frame_infos = zhash_pack(self->infos);
                    zmsg_append (reply, &frame_infos);
                    mlm_client_sendto (client, mlm_client_sender (client), "info", NULL, 1000, &reply);
                    zframe_destroy(&frame_infos);
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
    printf (" * fty_info_server_test: ");

    //  @selftest

   static const char* endpoint = "inproc://fty-info-test";

   zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
   zstr_sendx (server, "BIND", endpoint, NULL);
   if (verbose)
       zstr_send (server, "VERBOSE");

   mlm_client_t *client = mlm_client_new ();
   mlm_client_connect (client, endpoint, 1000, "fty_info_server_test");
   
   
   zactor_t *info_server = zactor_new (fty_info_server, (void*) "fty-info");
      if (verbose)
       zstr_send (info_server, "VERBOSE");
   zstr_sendx (info_server, "CONNECT", endpoint, NULL);
   zclock_sleep (1000);
 
    // Test #1: request INFO-TEST
    {
        zsys_debug ("fty-info-test:Test #1");
        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO-TEST");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "info", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 2);
        zsys_debug ("fty-info-test: zmsg_size = %d",zmsg_size (recv));
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos); 

        char * uuid = (char *) zhash_lookup (infos, INFO_UUID);
        assert(uuid && streq (uuid,TST_UUID));
        zsys_debug ("fty-info-test: uuid = '%s'", uuid);
        char * hostname = (char *) zhash_lookup (infos, INFO_HOSTNAME);
        assert(hostname && streq (hostname, TST_HOSTNAME));
        zsys_debug ("fty-info-test: hostname = '%s'", hostname);
        char * name = (char *) zhash_lookup (infos, INFO_NAME);
        assert(name && streq (name, TST_NAME));
        zsys_debug ("fty-info-test: name = '%s'", name);
        char * vendor = (char *) zhash_lookup (infos, INFO_VENDOR);
        assert(vendor && streq (vendor, TST_VENDOR));
        zsys_debug ("fty-info-test: vendor = '%s'", vendor);
        char * serial = (char *) zhash_lookup (infos, INFO_SERIAL);
        assert(serial && streq (serial, TST_SERIAL));
        zsys_debug ("fty-info-test: serial = '%s'", serial);
        char * model = (char *) zhash_lookup (infos, INFO_MODEL);
        assert(model && streq (model, TST_MODEL));
        zsys_debug ("fty-info-test: model = '%s'", model);
        char * location = (char *) zhash_lookup (infos, INFO_LOCATION);
        assert(location && streq (location, TST_LOCATION));
        zsys_debug ("fty-info-test: location = '%s'", location);
        char * version = (char *) zhash_lookup (infos, INFO_VERSION);
        assert(version && streq (version, TST_VERSION));
        zsys_debug ("fty-info-test: version = '%s'", version);
        char * rest_root = (char *) zhash_lookup (infos, INFO_REST_PATH);
        assert(rest_root && streq (rest_root, TST_PATH));
        zsys_debug ("fty-info-test: rest_path = '%s'", rest_root);
        char * rest_port = (char *) zhash_lookup (infos, INFO_REST_PORT);
        assert(rest_port && streq (rest_port, TST_PORT));
        zsys_debug ("fty-info-test: rest_port = '%s'", rest_port);
        zstr_free (&zuuid_reply);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #1: OK");
    }
   
    // Test #2: request INFO
    {
        zsys_debug ("fty-info-test:Test #2");
        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 2);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos); 

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )
        {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #2: OK");
    }

    //  @end
    zactor_destroy (&info_server);
    mlm_client_destroy (&client);
    zactor_destroy (&server);
    zsys_info ("OK\n");
    
}
