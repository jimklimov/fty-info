/*  =========================================================================
    topologyresolver - Class for asset location recursive resolving

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
    topologyresolver - Class for asset location recursive resolving
@discuss
@end
*/

#include "fty_info_classes.h"
#include <map>
#include <set>

// State

typedef enum {
    DISCOVERING = 0,
    UPTODATE
} ResolverState;

//  Structure of our class

struct _topologyresolver_t {
    char *iname;
    char *topology;
    int state;
    zhashx_t *assets;
};

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

//check if this is our rack controller - is any IP address
//of this asset the same as one of the local addresses?
bool is_this_me (fty_proto_t *asset)
{
    const char *operation = fty_proto_operation (asset);
    bool found = false;
    if (streq (operation, FTY_PROTO_ASSET_OP_CREATE) ||
        streq (operation, FTY_PROTO_ASSET_OP_UPDATE)) {
        //are we creating/updating a rack controller?
        const char *type = fty_proto_aux_string (asset, "type", "");
        const char *subtype = fty_proto_aux_string (asset, "subtype", "");
        if (streq (type, "device") || streq (subtype, "rackcontroller")) {
            auto ifaces = s_local_addresses ();
            zhash_t *ext = fty_proto_ext (asset);

            int ipv6_index = 1;
            found = false;
            while (true) {
                void *ip = zhash_lookup (ext, ("ipv6." + std::to_string (ipv6_index)).c_str ());
                ipv6_index++;
                if (ip != NULL) {
                    for (auto &iface : ifaces) {
                        if (iface.second.find ((char *)ip) != iface.second.end ()) {
                            found = true;
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
    }
    return found;
}

//  --------------------------------------------------------------------------
//  Create a new topologyresolver

topologyresolver_t *
topologyresolver_new (const char *iname)
{
    topologyresolver_t *self = (topologyresolver_t *) zmalloc (sizeof (topologyresolver_t));
    assert (self);
    //  Initialize class properties here
    if (iname) self->iname = strdup (iname);
    self->assets = zhashx_new ();
    zhashx_set_destructor (self->assets, (czmq_destructor *) fty_proto_destroy);
    zhashx_set_duplicator (self->assets, (czmq_duplicator *) fty_proto_dup);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the topologyresolver

void
topologyresolver_destroy (topologyresolver_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        topologyresolver_t *self = *self_p;
        //  Free class properties here
        zhashx_destroy (&self->assets);
        zstr_free (&self->iname);
        zstr_free (&self->topology);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Give topology resolver one asset information
void
topologyresolver_asset (topologyresolver_t *self, fty_proto_t *message)
{
    if (! self || ! message) return;
    if (fty_proto_id (message) != FTY_PROTO_ASSET) return;

    if (!self->iname && is_this_me (message)) {
        self->iname = strdup (fty_proto_name (message));
    }
    const char *iname = fty_proto_name (message);
    zhashx_update (self->assets, iname, message);
}

//  --------------------------------------------------------------------------
// Return URI of asset for this topologyresolver
FTY_INFO_PRIVATE const char *
    topologyresolver_to_name_uri (topologyresolver_t *self)
{
    if (self->iname) {
        std::string asset("/asset/");
        return (asset + self->iname).c_str ();
    }
    else
        return "NA";
}

//  --------------------------------------------------------------------------
//  Return URI of the asset's parent
const char *
topologyresolver_to_parent_uri (topologyresolver_t *self)
{
    if (self->iname) {
        fty_proto_t *rc_message = (fty_proto_t *) zhashx_lookup (self->assets, self->iname);
        if (rc_message) {
            const char *parent_uri = fty_proto_aux_string (rc_message, "parent", "NA");
            if (!streq (parent_uri, "NA")) {
                std::string asset("/asset/");
                return (asset + parent_uri).c_str ();
            }
            else
                return "NA";
        }
        else
            return "NA";
    }
    else
        return "NA";
}


//  --------------------------------------------------------------------------
//  Return user-friendly name of the asset
const char *
topologyresolver_to_name (topologyresolver_t *self)
{
    if (self->iname) {
        fty_proto_t *rc_message = (fty_proto_t *) zhashx_lookup (self->assets, self->iname);
        if (rc_message)
            return fty_proto_ext_string (rc_message, "name", "NA");
        else
            return "NA";
    }
    else
        return "NA";
}

//  --------------------------------------------------------------------------
//  Return topology as string of friendly names (or NULL if incomplete)
const char *
topologyresolver_to_string (topologyresolver_t *self, const char *separator)
{
    zlistx_t *parents = topologyresolver_to_list (self);
    char *next = (char *) zlistx_first (parents);
    if (!next) {
        zlistx_destroy (&parents);
        return "NA";
    }
    else {
        zstr_free (&self->topology);
        self->topology = strdup ("");
        char *iname = (char *) zlistx_first (parents);
        while (iname) {
            fty_proto_t *msg = (fty_proto_t *) zhashx_lookup (self->assets, iname);
            if (msg) {
                const char *ename = fty_proto_ext_string (msg, "name", iname);
                char *tmp = zsys_sprintf ("%s%s%s", self->topology, ename, separator);
                if (tmp) {
                    zstr_free (&self->topology);
                    self->topology = tmp;
                }
            }
            iname = (char *) zlistx_next (parents);
        };
        if (strlen (self->topology) >= strlen (separator)) {
            // remove trailing separator
            char *p = &self->topology [strlen (self->topology) - strlen (separator)];
            *p = 0;
        }
        zlistx_destroy (&parents);
        return (const char *) self->topology;
    }
}



//  --------------------------------------------------------------------------
//  Return zlist of inames starting with asset up to DC
//  Empty list is returned if the topology is incomplete yet
zlistx_t *
topologyresolver_to_list (topologyresolver_t *self)
{
    zlistx_t *list = zlistx_new();
    zlistx_set_destructor (list, (void (*)(void**))zstr_free);
    zlistx_set_duplicator (list, (void* (*)(const void*))strdup);

    if (!self || !self->iname) return list;
    fty_proto_t *msg = (fty_proto_t *) zhashx_lookup (self->assets, self->iname);
    if (!msg)
        return list;

    char buffer[16]; // strlen ("parent_name.123") + 1

    for (int i=1; i<100; i++) {
        snprintf (buffer, 16, "parent_name.%i", i);
        const char *parent = fty_proto_aux_string (msg, buffer, NULL);
        if (! parent) break;
        if (! zhashx_lookup (self->assets, parent)) {
            // parent is unknown, topology is not complete
            zlistx_purge (list);
            break;
        } else {
            zlistx_add_start (list, (void *)parent);
        }
    }

    return list;
}

void
topologyresolver_test (bool verbose)
{
    printf (" * topologyresolver_test: ");
    topologyresolver_t *resolver = topologyresolver_new ("me");
    fty_proto_t *msg = fty_proto_new (FTY_PROTO_ASSET);
    fty_proto_set_name (msg, "grandparent");
    zhash_t *ext = zhash_new ();
    zhash_autofree (ext);
    zhash_update (ext, "name", (void *)"my nice grandparent");
    fty_proto_set_ext (msg, &ext);

    fty_proto_t *msg2 = fty_proto_new (FTY_PROTO_ASSET);
    fty_proto_set_name (msg2, "me");
    ext = zhash_new ();
    zhash_autofree (ext);
    zhash_update (ext, "name", (void *)"this is me");
    fty_proto_set_ext (msg2, &ext);
    zhash_t *aux = zhash_new ();
    zhash_autofree (aux);
    zhash_update (aux, "parent_name.1", (void *)"parent");
    zhash_update (aux, "parent_name.2", (void *)"grandparent");
    fty_proto_set_aux (msg2, &aux);

    fty_proto_t *msg3 = fty_proto_new (FTY_PROTO_ASSET);
    fty_proto_set_name (msg3, "parent");
    ext = zhash_new ();
    zhash_autofree (ext);
    zhash_update (ext, "name", (void *)"this is father");
    fty_proto_set_ext (msg3, &ext);
    aux = zhash_new ();
    zhash_autofree (aux);
    zhash_update (aux, "parent_name.1", (void *)"grandparent");
    fty_proto_set_aux (msg3, &aux);

    assert (streq (topologyresolver_to_string (resolver), "NA"));
    topologyresolver_asset (resolver, msg);
    topologyresolver_asset (resolver, msg2);
    assert (streq (topologyresolver_to_string (resolver), "NA"));
    topologyresolver_asset (resolver, msg3);
    assert (streq ("my nice grandparent->this is father", topologyresolver_to_string (resolver, "->")));
    fty_proto_destroy(&msg3);
    fty_proto_destroy(&msg2);
    fty_proto_destroy(&msg);
    topologyresolver_destroy (&resolver);
    printf ("OK\n");
}
