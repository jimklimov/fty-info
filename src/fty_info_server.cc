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
#define TIMEOUT_MS -1   //wait infinitelly

static const char* RELEASE_DETAILS = "/etc/release-details.json";

//default values
#define SRV_NAME "IPC"
#define SRV_TYPE "_https._tcp."
#define SRV_STYPE "_powerservice._sub._https._tcp."
#define SRV_PORT "443"
#define TXT_PATH "/api/v1/comm/connections"
#define TXT_PROTO_FORMAT "etnrs"
#define TXT_TYPE   "ipc"
#define TXT_VER  "1"
//test value for INFO-TEST command reply    
#define TST_UUID     "ce7c523e-08bf-11e7-af17-080027d52c4f"
#define TST_HOSTNAME "localhost"
#define TST_NAME     "ipc-001"
#define TST_MODEL    "IPC3000"
#define TST_VENDOR   "Eaton"
#define TST_SERIAL   "LA71026006"
#define TST_LOCATION "Rack1"
#define TST_LOCATION2 "Rack2"
#define TST_VERSION  "1.0.0"



#include <string>
#include <unistd.h>
#include <bits/local_lim.h>
#include <cxxtools/jsondeserializer.h>
#include <istream>
#include <fstream>
#include <set>
#include <map>
#include <ifaddrs.h>

#include "fty_info_classes.h"

struct _fty_info_t {
    zhash_t *infos;
    char *uuid;
    char *hostname;
    char *iname;
    char *model;
    char *vendor;
    char *serial;
    char *location;
    char *version;
    char *path;
    char *protocol_format;
    char *type;
    char *txtvers;
};

struct _fty_info_server_t {
    //  Declare class properties here
    char* name;
    char* endpoint;
    mlm_client_t *client;
    mlm_client_t *announce_client;
    bool verbose;
    bool first_announce;
    bool announce_test;
    fty_proto_t* rc_message;
    fty_proto_t* parent_message;
};

//  --------------------------------------------------------------------------
//  Create a new mango_profpga_config_server

fty_info_server_t  *
info_server_new (char *name)
{
    fty_info_server_t  *self = (fty_info_server_t  *) zmalloc (sizeof (fty_info_server_t ));
    assert (self);
    //  Initialize class properties here
    self->name=strdup(name);
    self->client = mlm_client_new ();
    self->announce_client = mlm_client_new ();
    self->verbose=false;
    self->first_announce=true;
    self->announce_test=false;
    return self;
}
//  --------------------------------------------------------------------------
//  Destroy the mango_profpga_config_server

void
info_server_destroy (fty_info_server_t  **self_p)
{
    assert (self_p);
    if (*self_p) {
        fty_info_server_t  *self = *self_p;
        //  Free class properties here
        mlm_client_destroy (&self->client);
        mlm_client_destroy (&self->announce_client);
        if(self->name)
            zstr_free(&self->name);
        if(self->endpoint)
            zstr_free(&self->endpoint);
        if(self->rc_message)
            fty_proto_destroy (&self->rc_message);
        if(self->parent_message)
            fty_proto_destroy (&self->parent_message);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

static std::map<std::string,std::set<std::string>>
s_local_addresses()
{
    struct ifaddrs *interfaces, *iface;
    char host[NI_MAXHOST];
    std::map<std::string,std::set<std::string>> result;

    if (getifaddrs (&interfaces) == -1) {
        return result;
    }
    iface = interfaces;
    for (iface = interfaces; iface != NULL; iface = iface->ifa_next) {
        if (iface->ifa_addr == NULL) continue;
        int family = iface->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            if (
                    getnameinfo(iface->ifa_addr,
                        (family == AF_INET) ? sizeof(struct sockaddr_in) :
                        sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST,
                        NULL, 0, NI_NUMERICHOST) == 0
               ) {
                // sometimes IPv6 addres looks like ::2342%IfaceName
                char *p = strchr (host, '%');
                if (p) *p = 0;

                auto it = result.find (iface->ifa_name);
                if (it == result.end()) {
                    std::set<std::string> aSet;
                    aSet.insert (host);
                    result [iface->ifa_name] = aSet;
                } else {
                    result [iface->ifa_name].insert (host);
                }
            }
        }
    }
    freeifaddrs (interfaces);
    return result;
}

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

//return IPC (uuid first 8 digits)
// the returned buffer should be freed
static 
char *s_get_name(const char *name, const char *uuid)
{
    
    char *buffer = (char*)malloc(strlen(name)+12);
    char first_digit[9];
    strncpy ( first_digit, uuid, 8 );
    first_digit[8]='\0';
    sprintf(buffer, "%s (%s)",name,first_digit);
    return buffer;
}

fty_info_t*
fty_info_test_new (void)
{
    fty_info_t *self = (fty_info_t *) zmalloc (sizeof (fty_info_t));
    // TXT attributes
    self->infos     = zhash_new();
    self->uuid      = strdup (TST_UUID);
    self->hostname  = strdup (TST_HOSTNAME);
    self->iname     = strdup (TST_NAME);
    self->model     = strdup (TST_MODEL);
    self->vendor    = strdup (TST_VENDOR);
    self->serial    = strdup (TST_SERIAL);
    self->location  = strdup (TST_LOCATION);
    self->version   = strdup (TST_VERSION);
    self->path      = strdup (TXT_PATH);
    self->protocol_format = strdup (TXT_PROTO_FORMAT);
    self->type      = strdup (TXT_TYPE);
    self->txtvers   = strdup (TXT_VER);
    return self;
}

fty_info_t*
fty_info_new (fty_proto_t *rc_message, fty_proto_t *parent_message)
{
    fty_info_t *self = (fty_info_t *) zmalloc (sizeof (fty_info_t));
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

    //set name
    if (rc_message != NULL)
        self->iname = strdup (fty_proto_name (rc_message));
    else
        self->iname = strdup ("NA");
    zsys_info ("fty-info:name      = '%s'", self-> iname);

    //set location (parent)
    if (rc_message != NULL)
        self->location  = strdup (fty_proto_aux_string (rc_message, "parent", "NA"));
    else
        self->location  = strdup ("NA");
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
    self->path = strdup (TXT_PATH);
    self->protocol_format = strdup (TXT_PROTO_FORMAT);
    self->type = strdup (TXT_TYPE);
    self->txtvers   = strdup (TXT_VER);
    zsys_info ("fty-info:version = '%s'", self->version);
    zsys_info ("fty-info:path = '%s'", self->path);
    zsys_info ("fty-info:protocol_format = '%s'", self->protocol_format);
    zsys_info ("fty-info:type = '%s'", self->type);
    zsys_info ("fty-info:txtvers = '%s'", self->txtvers);

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
        zstr_free (&self->iname);
        zstr_free (&self->model);
        zstr_free (&self->vendor);
        zstr_free (&self->serial);
        zstr_free (&self->location);
        zstr_free (&self->version);
        zstr_free (&self->path);
        zstr_free (&self->protocol_format);
        zstr_free (&self->type);
        zstr_free (&self->txtvers);
        // Free object itself
        free (self);
        *self_ptr = NULL;
    }

}

//  --------------------------------------------------------------------------
//  publish announcement on STREAM ANNOUNCE/ANNOUNCE-TEST
//  subject : CREATE/UPDATE
//  body :
//    - name    IPC (XXXXXXXX)
//    - type    _https._tcp.
//    - subtype _powerservice._sub._https._tcp.
//    - port    443
//    - hashtable : TXT name, TXT value
//          uuid
//          name
//          vendor
//          serial
//          model
//          location
//          version
//          path
//          protocol format
//          type
//          version
static void
s_publish_announce(fty_info_server_t  * self)
{

    if(!mlm_client_connected(self->announce_client))
        return;
    fty_info_t *info;
    if (!self->announce_test) {
        info = fty_info_new (self->rc_message, self->parent_message);
    }
    else
        info = fty_info_test_new ();

    //prepare  msg content
    zmsg_t *msg=zmsg_new();
    char *srv_name = s_get_name(SRV_NAME, info->uuid);
    zmsg_addstr (msg, srv_name);
    zmsg_addstr (msg, SRV_TYPE);
    zmsg_addstr (msg, SRV_STYPE);
    zmsg_addstr (msg, SRV_PORT);

    zhash_t *map = zhash_new ();
    zhash_autofree (map);
    zhash_insert(map, INFO_UUID, info->uuid);
    zhash_insert(map, INFO_HOSTNAME, info->hostname);
    zhash_insert(map, INFO_NAME, info->iname);
    zhash_insert(map, INFO_VENDOR, info->vendor);
    zhash_insert(map, INFO_MODEL, info->model);
    zhash_insert(map, INFO_SERIAL, info->serial);
    zhash_insert(map, INFO_LOCATION, info->location);
    zhash_insert(map, INFO_VERSION, info->version);
    zhash_insert(map, INFO_REST_PATH, info->path);
    zhash_insert(map, INFO_PROTOCOL_FORMAT, info->protocol_format);
    zhash_insert(map, INFO_TYPE, info->type);
    zhash_insert(map, INFO_TXTVERS, info->txtvers);

    zframe_t * frame_infos = zhash_pack(map);
    zmsg_append (msg, &frame_infos);
    if (self->first_announce) {
        if (mlm_client_send (self->announce_client, "CREATE", &msg) != -1) {
            zsys_info("publish CREATE msg on ANNOUNCE STREAM");
            self->first_announce=false;
        }
        else
            zsys_error("cant publish CREATE msg on ANNOUNCE STREAM");
    } else {
        if (mlm_client_send (self->announce_client, "UPDATE", &msg) != -1)
            zsys_info("publish UPDATE msg on ANNOUNCE STREAM");
        else
            zsys_error("cant publish UPDATE msg on ANNOUNCE STREAM");
    }
    zstr_free(&srv_name);
    zframe_destroy(&frame_infos);
    zhash_destroy (&map);
    fty_info_destroy (&info);
}
//  --------------------------------------------------------------------------
//  process pipe message
//  return true means continue, false means TERM
bool static
s_handle_pipe(fty_info_server_t* self,zmsg_t *message)
{
    if (!message)
        return true;
    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (&message);
        zsys_warning ("Empty command.");
        return true;
    }
    if (streq(command, "$TERM")) {
        zsys_info ("Got $TERM");
        zmsg_destroy (&message);
        zstr_free (&command);
        return false;
    }
    else
    if (streq(command, "CONNECT")) {
        char *endpoint = zmsg_popstr (message);

        if (endpoint) {
            self->endpoint = strdup(endpoint);
            zsys_debug ("fty-info: CONNECT: %s/%s", self->endpoint, self->name);
            int rv = mlm_client_connect (self->client, self->endpoint, 1000, self->name);
            if (rv == -1)
                zsys_error("mlm_client_connect failed\n");

        }
        zstr_free (&endpoint);
    }
    else
    if (streq (command, "VERBOSE")) {
        self->verbose = true;
        zsys_debug ("fty-info: VERBOSE=true");
    }
    else
    if (streq (command, "CONSUMER")) {
        char* stream = zmsg_popstr (message);
        char* pattern = zmsg_popstr (message);
        int rv = mlm_client_set_consumer (self->client, stream, pattern);
        if (rv == -1)
            zsys_error ("%s: can't set consumer on stream '%s', '%s'",
                    self->name, stream, pattern);
        zstr_free (&pattern);
        zstr_free (&stream);
    }
    else
    if (streq (command, "PRODUCER")) {
        char* stream = zmsg_popstr (message);
        self->announce_test=streq(stream,"ANNOUNCE-TEST");
        int rv = mlm_client_connect (self->announce_client, self->endpoint, 1000, "fty_info_announce");
        if (rv == -1)
                zsys_error("fty_info_announce : mlm_client_connect failed\n");
        rv = mlm_client_set_producer (self->announce_client, stream);
        if (rv == -1)
            zsys_error ("%s: can't set producer on stream '%s'",
                    self->name, stream);
        else
            //do the first announce
            s_publish_announce(self);
        zstr_free (&stream);
    }
    else if (streq (command, "ANNOUNCE")) {
        s_publish_announce (self);
    }
    else
        zsys_error ("fty-info: Unknown actor command: %s.\n", command);

    zstr_free (&command);
    zmsg_destroy (&message);
    return true;
}

//  --------------------------------------------------------------------------
//  process message from FTY_PROTO_ASSET stream
void static
s_handle_stream(fty_info_server_t* self,zmsg_t *message)
{
    if (!is_fty_proto (message)){
        zmsg_destroy (&message);
        return;
    }
    fty_proto_t *bmessage = fty_proto_decode (&message);
    if (!bmessage ) {
        zsys_error ("can't decode message with subject %s, ignoring", mlm_client_subject (self->client));
        zmsg_destroy (&message);
        return;
    }
    if (fty_proto_id (bmessage) != FTY_PROTO_ASSET) {
        fty_proto_destroy (&bmessage);
        zmsg_destroy (&message);
        return;

    }
    //check whether the message is relevant for us
    const char *operation = fty_proto_operation (bmessage);
    bool found = false;
    if (streq (operation, FTY_PROTO_ASSET_OP_CREATE) ||
        streq (operation, FTY_PROTO_ASSET_OP_UPDATE)) {
        //are we creating/updating a rack controller?
        const char *type = fty_proto_aux_string (bmessage, "type", "");
        const char *subtype = fty_proto_aux_string (bmessage, "subtype", "");
        if (streq (type, "device") || streq (subtype, "rackcontroller")) {
            //check if this is our rack controller - is any IP address
            //of this asset the same as one of the local addresses?
            auto ifaces = s_local_addresses ();
            zhash_t *ext = fty_proto_ext (bmessage);

            int ipv6_index = 1;
            found = false;
            while (true) {
                void *ip = zhash_lookup (ext, ("ipv6." + std::to_string (ipv6_index)).c_str ());
                ipv6_index++;
                if (ip != NULL) {
                    for (auto &iface : ifaces) {
                        if (iface.second.find ((char *)ip) != iface.second.end ()) {
                            found = true;
                            if(self->rc_message)
                                fty_proto_destroy (&(self->rc_message));
                            self->rc_message = fty_proto_dup (bmessage);
                            //try another network interface only if match was not found
                            break;
                        }
                    }
                    // try another address only if match was not found
                    if (found)
                        break;
                }
                // no other IPv6 address on the investigated asset
                else
                    break;
            }

            found = false;
            int ipv4_index = 1;
            while (true) {
                void *ip = zhash_lookup (ext, ("ip." + std::to_string (ipv4_index)).c_str ());
                ipv4_index++;
                if (ip != NULL) {
                    for (auto &iface : ifaces) {
                        if (iface.second.find ((char *)ip) != iface.second.end ()) {
                            found = true;
                            if(self->rc_message)
                                fty_proto_destroy (&(self->rc_message));
                            self->rc_message = fty_proto_dup (bmessage);
                            //try another network interface only if match was not found
                            break;
                        }
                    }
                    // try another address only if match was not found
                    if (found)
                        break;
                }
                // no other IPv4 address on the investigated asset
                else
                    break;
            }
        }
        const char *name = fty_proto_name (bmessage);
        if (self->rc_message == NULL || streq (name, fty_proto_aux_string (self->rc_message, "parent", ""))) {
            // not used right now - location is internal asset name which does not changes
            if(self->parent_message)
                fty_proto_destroy (&(self->parent_message));
            self->parent_message = fty_proto_dup (bmessage);
            found=true;
        }
        if(found)
            s_publish_announce(self);
    }

    fty_proto_destroy (&bmessage);
    zmsg_destroy (&message);

}

//  --------------------------------------------------------------------------
//  process message from MAILBOX DELIVER INFO INFO/INFO-TEST
//  body :
//    - name    IPC (12378)
//    - type    _https._tcp.
//    - subtype _powerservice._sub._https._tcp.
//    - port    443
//    - hashtable : TXT name, TXT value
//          uuid
//          name
//          vendor
//          serial
//          model
//          location
//          version
//          path
//          protocol format
//          type
//          version
void static
s_handle_mailbox(fty_info_server_t* self,zmsg_t *message)
{
    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (&message);
        zsys_warning ("Empty subject.");
        return;
    }
    //we assume all request command are MAILBOX DELIVER, and subject="info"
    if (!streq(command, "INFO") && !streq(command, "INFO-TEST")) {
        zsys_warning ("fty-info: Received unexpected command '%s'", command);
        zmsg_t *reply = zmsg_new ();
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr (reply, "unexpected command");
        mlm_client_sendto (self->client, mlm_client_sender (self->client), "info", NULL, 1000, &reply);
        zstr_free (&command);
        zmsg_destroy (&message);
        return;
    }
    else {
        zsys_debug ("fty-info:do '%s'", command);
        zmsg_t *reply = zmsg_new ();
        char *zuuid = zmsg_popstr (message);
        fty_info_t *info;
        if (streq(command, "INFO")) {
            info = fty_info_new (self->rc_message, self->parent_message);
        }
        if (streq(command, "INFO-TEST")) {
            info = fty_info_test_new ();
        }
        //prepare replied msg content
        zmsg_addstrf (reply, "%s", zuuid);
        char *srv_name = s_get_name(SRV_NAME, info->uuid);
        zmsg_addstr (reply, srv_name);
        zmsg_addstr (reply, SRV_TYPE);
        zmsg_addstr (reply, SRV_STYPE);
        zmsg_addstr (reply, SRV_PORT);
        zhash_insert(info->infos, INFO_UUID, info->uuid);
        zhash_insert(info->infos, INFO_HOSTNAME, info->hostname);
        zhash_insert(info->infos, INFO_NAME, info->iname);
        zhash_insert(info->infos, INFO_VENDOR, info->vendor);
        zhash_insert(info->infos, INFO_MODEL, info->model);
        zhash_insert(info->infos, INFO_SERIAL, info->serial);
        zhash_insert(info->infos, INFO_LOCATION, info->location);
        zhash_insert(info->infos, INFO_VERSION, info->version);
        zhash_insert(info->infos, INFO_REST_PATH, info->path);
        zhash_insert(info->infos, INFO_PROTOCOL_FORMAT, info->protocol_format);
        zhash_insert(info->infos, INFO_TYPE, info->type);
        zhash_insert(info->infos, INFO_TXTVERS, info->txtvers);

        zframe_t * frame_infos = zhash_pack(info->infos);
        zmsg_append (reply, &frame_infos);
        mlm_client_sendto (self->client, mlm_client_sender (self->client), "info", NULL, 1000, &reply);
        zframe_destroy(&frame_infos);
        zstr_free (&zuuid);
        zstr_free(&srv_name);
        fty_info_destroy (&info);
    }
    zstr_free (&command);
    zmsg_destroy (&message);

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

    fty_info_server_t *self = info_server_new (name);
    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (self->client), NULL);
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
            if (self->verbose)
                zsys_debug ("which == pipe");
            if(!s_handle_pipe(self,zmsg_recv (pipe)))
                break;//TERM
            else continue;
        }
        else
        if (which == mlm_client_msgpipe (self->client)) {
            zmsg_t *message = mlm_client_recv (self->client);
            if (!message)
                continue;
            const char *command = mlm_client_command (self->client);
            if (streq (command, "STREAM DELIVER")) {
                s_handle_stream (self, message);
            }
            else
            if (streq (command, "MAILBOX DELIVER")) {
                s_handle_mailbox (self, message);
            }
        }
    }

    zpoller_destroy (&poller);
    info_server_destroy(&self);
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
    zstr_sendx (info_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
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

        assert (zmsg_size (recv) == 6);
        zsys_debug ("fty-info-test: zmsg_size = %d",zmsg_size (recv));
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        
        char *srv_name = zmsg_popstr (recv);
        assert (srv_name && streq (srv_name,"IPC (ce7c523e)"));
        zsys_debug ("fty-info-test: srv name = '%s'", srv_name);
        char *srv_type = zmsg_popstr (recv);
        assert (srv_type && streq (srv_type,SRV_TYPE));
        zsys_debug ("fty-info-test: srv type = '%s'", srv_type);
        char *srv_stype = zmsg_popstr (recv);
        assert (srv_stype && streq (srv_stype,SRV_STYPE));
        zsys_debug ("fty-info-test: srv stype = '%s'", srv_stype);
        char *srv_port = zmsg_popstr (recv);
        assert (srv_port && streq (srv_port,SRV_PORT));
        zsys_debug ("fty-info-test: srv port = '%s'", srv_port);

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
        assert(rest_root && streq (rest_root, TXT_PATH));
        zsys_debug ("fty-info-test: rest_path = '%s'", rest_root);
        zstr_free (&zuuid_reply);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
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

        assert (zmsg_size (recv) == 6);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));

        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);
        
        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #2: OK");
    }
    mlm_client_t *asset_generator = mlm_client_new ();
    mlm_client_connect (asset_generator, endpoint, 1000, "fty_info_asset_generator");
    mlm_client_set_producer (asset_generator, FTY_PROTO_STREAM_ASSETS);
    // Test #3: process asset message - CREATE RC
    {
        zsys_debug ("fty-info-test:Test #3");
        const char *location = TST_LOCATION;
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        zhash_update (aux, "type", (void *) "device");
	zhash_update (aux, "subtype", (void *) "rackcontroller");
	zhash_update (aux, "parent", (void *) location);
        zhash_update (ext, "ip.1", (void *) "127.0.0.1");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_NAME,
                FTY_PROTO_ASSET_OP_CREATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rackcontroller@ipc-001", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 6);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
		if (streq (key, "name"))
			assert (streq (value, TST_NAME));
		if (streq (key, "location"))
			assert (streq (value, TST_LOCATION));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #3: OK");
    }
    //TEST #4: process asset message - UPDATE RC (change location)
    {
        zsys_debug ("fty-info-test:Test #4");
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        const char *location = TST_LOCATION2;
        zhash_update (aux, "type", (void *) "device");
	zhash_update (aux, "subtype", (void *) "rackcontroller");
        zhash_update (aux, "parent", (void *) location);
        zhash_update (ext, "ip.1", (void *) "127.0.0.1");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_NAME,
                FTY_PROTO_ASSET_OP_UPDATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rackcontroller@ipc-001", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 6);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);
        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
		if (streq (key, "name"))
			assert (streq (value, TST_NAME));
		if (streq (key, "location"))
			assert (streq (value, TST_LOCATION2));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #4: OK");
    }
    //TEST #5: process asset message - do not process CREATE RC with IP address
    // which does not belong to us
    {
        zsys_debug ("fty-info-test:Test #5");
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        const char *location = TST_LOCATION;
        zhash_update (aux, "type", (void *) "device");
        zhash_update (aux, "subtype", (void *) "rack controller");
        zhash_update (aux, "parent", (void *) location);
        // use invalid IP address to make sure we don't have it
        zhash_update (ext, "ip.1", (void *) "300.3000.300.300");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_NAME,
                FTY_PROTO_ASSET_OP_CREATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rack controller@ipc-001", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 6);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
		if (streq (key, "name"))
			assert (streq (value, TST_NAME));
		if (streq (key, "location"))
			assert (streq (value, TST_LOCATION2));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #5: OK");
    }
    // TEST #6 : test STREAM announce
    {
        zsys_debug ("fty-info-test:Test #6");
        int rv = mlm_client_set_consumer (client, "ANNOUNCE-TEST", ".*");
        assert(rv>=0);
        zstr_sendx (info_server, "PRODUCER", "ANNOUNCE-TEST", NULL);
        zmsg_t *recv = mlm_client_recv (client);
        assert(recv);
        const char *command = mlm_client_command (client);
        assert(streq (command, "STREAM DELIVER"));
        char *srv_name = zmsg_popstr (recv);
        assert (srv_name && streq (srv_name,"IPC (ce7c523e)"));
        zsys_debug ("fty-info-test: srv name = '%s'", srv_name);
        char *srv_type = zmsg_popstr (recv);
        assert (srv_type && streq (srv_type,SRV_TYPE));
        zsys_debug ("fty-info-test: srv type = '%s'", srv_type);
        char *srv_stype = zmsg_popstr (recv);
        assert (srv_stype && streq (srv_stype,SRV_STYPE));
        zsys_debug ("fty-info-test: srv stype = '%s'", srv_stype);
        char *srv_port = zmsg_popstr (recv);
        assert (srv_port && streq (srv_port,SRV_PORT));
        zsys_debug ("fty-info-test: srv port = '%s'", srv_port);

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
        assert(rest_root && streq (rest_root, TXT_PATH));
        zsys_debug ("fty-info-test: rest_path = '%s'", rest_root);
        
        
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);

        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zsys_info ("fty-info-test:Test #6: OK");

    }
    mlm_client_destroy (&asset_generator);
    //  @end
    zactor_destroy (&info_server);
    mlm_client_destroy (&client);
    zactor_destroy (&server);
    zsys_info ("OK\n");
}
