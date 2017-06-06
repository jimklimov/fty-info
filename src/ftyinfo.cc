/*  =========================================================================
    ftyinfo - Class for keeping fty information

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
    ftyinfo - Class for keeping fty information
@discuss
@end
*/

#include "fty_info_classes.h"

#include <cxxtools/jsondeserializer.h>
#include <istream>
#include <fstream>
#include <set>
#include <map>

//  Structure of our class

struct _ftyinfo_t {
    zhash_t *infos;
    char *id;
    char *uuid;
    char *hostname;
    char *name;
    char *name_uri;
    char *model;
    char *vendor;
    char *serial;
    char *location;
    char *parent_uri;
    char *version;
    char *path;
    char *protocol_format;
    char *type;
    char *txtvers;
};

static const char* RELEASE_DETAILS = "/etc/release-details.json";

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

//  --------------------------------------------------------------------------
//  Create a new ftyinfo

ftyinfo_t *
ftyinfo_new (topologyresolver_t *resolver)
{
    ftyinfo_t *self = (ftyinfo_t *) zmalloc (sizeof (ftyinfo_t));
    self->infos = zhash_new();

    // set hostname
    char *hostname = (char *) malloc (HOST_NAME_MAX+1);
    int rv = gethostname (hostname, HOST_NAME_MAX+1);
    if (rv == -1) {
        zsys_warning ("ftyinfo could not be fully initialized (error while getting the hostname)");
        self->hostname = strdup("locahost");
    }
    else {
        self->hostname = strdup (hostname);
    }
    zstr_free (&hostname);
    zsys_info ("fty-info:hostname  = '%s'", self->hostname);

    //set id
    self->id = strdup (topologyresolver_id (resolver));
    zsys_info ("fty-info:id        = '%s'", self->id);

    //set name
    self->name = topologyresolver_to_rc_name (resolver);
    zsys_info ("fty-info:name      = '%s'", self-> name);

    //set name_uri
    self->name_uri = topologyresolver_to_rc_name_uri (resolver);
    zsys_info ("fty-info:name_uri  = '%s'", self-> name_uri);

    //set location
    self->location = strdup (topologyresolver_to_string (resolver, ">"));
    zsys_info ("fty-info:location  = '%s'", self->location);

    //set parent_uri
    self->parent_uri = topologyresolver_to_parent_uri (resolver);
    zsys_info ("fty-info:parent_uri= '%s'", self->parent_uri);

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

//  --------------------------------------------------------------------------
//  Create a new ftyinfo for tests

ftyinfo_t *
ftyinfo_test_new (void)
{
    ftyinfo_t *self = (ftyinfo_t *) zmalloc (sizeof (ftyinfo_t));
    // TXT attributes
    self->infos     = zhash_new();
    self->id        = strdup (TST_ID);
    self->uuid      = strdup (TST_UUID);
    self->hostname  = strdup (TST_HOSTNAME);
    self->name      = strdup (TST_NAME);
    self->name_uri  = strdup (TST_NAME_URI);
    self->model     = strdup (TST_MODEL);
    self->vendor    = strdup (TST_VENDOR);
    self->serial    = strdup (TST_SERIAL);
    self->location  = strdup (TST_LOCATION);
    self->parent_uri  = strdup (TST_PARENT_URI);
    self->version   = strdup (TST_VERSION);
    self->path      = strdup (TXT_PATH);
    self->protocol_format = strdup (TXT_PROTO_FORMAT);
    self->type      = strdup (TXT_TYPE);
    self->txtvers   = strdup (TXT_VER);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the ftyinfo

void
ftyinfo_destroy (ftyinfo_t **self_ptr)
{
    if (!self_ptr)
        return;
    if (*self_ptr) {
        ftyinfo_t *self = *self_ptr;
        // Free class properties here
        zhash_destroy(&self->infos);
        zstr_free (&self->id);
        zstr_free (&self->uuid);
        zstr_free (&self->hostname);
        zstr_free (&self->name);
        zstr_free (&self->name_uri);
        zstr_free (&self->model);
        zstr_free (&self->vendor);
        zstr_free (&self->serial);
        zstr_free (&self->location);
        zstr_free (&self->parent_uri);
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
//  getters

const char * ftyinfo_uuid (ftyinfo_t *self)
{
    if (!self) return NULL;
    return self->uuid;
}

zhash_t *ftyinfo_infohash (ftyinfo_t *self)
{
    if (!self) return NULL;
    zhash_destroy (&self->infos);
    self->infos = zhash_new ();

    zhash_insert(self->infos, INFO_UUID, self->uuid);
    zhash_insert(self->infos, INFO_HOSTNAME, self->hostname);
    zhash_insert(self->infos, INFO_NAME, self->name);
    zhash_insert(self->infos, INFO_NAME_URI, self->name_uri);
    zhash_insert(self->infos, INFO_VENDOR, self->vendor);
    zhash_insert(self->infos, INFO_MODEL, self->model);
    zhash_insert(self->infos, INFO_SERIAL, self->serial);
    zhash_insert(self->infos, INFO_LOCATION, self->location);
    zhash_insert(self->infos, INFO_PARENT_URI, self->parent_uri);
    zhash_insert(self->infos, INFO_VERSION, self->version);
    zhash_insert(self->infos, INFO_REST_PATH, self->path);
    zhash_insert(self->infos, INFO_PROTOCOL_FORMAT, self->protocol_format);
    zhash_insert(self->infos, INFO_TYPE, self->type);
    zhash_insert(self->infos, INFO_TXTVERS, self->txtvers);

    return self->infos;
}
