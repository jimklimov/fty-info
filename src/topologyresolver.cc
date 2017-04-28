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

// State

typedef enum {
    DISCOVERING = 0,
    UPTODATE
} ResolverState;

//  Structure of our class

struct _topologyresolver_t {
    char *iname;
    int state;
    zhashx_t *assets;
};


//  --------------------------------------------------------------------------
//  Create a new topologyresolver

topologyresolver_t *
topologyresolver_new (const char *iname)
{
    if (! iname) return NULL;
    topologyresolver_t *self = (topologyresolver_t *) zmalloc (sizeof (topologyresolver_t));
    assert (self);
    //  Initialize class properties here
    self->iname = strdup (iname);
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
    if (! message) return;
    if (fty_proto_id (message) != FTY_PROTO_ASSET) return;

    const char *iname = fty_proto_name (message);
    zhashx_update (self->assets, iname, message);
}

//  --------------------------------------------------------------------------
//  Return topology as string of friedly names (or NULL if incomplete)
const char *
topologyresolver_to_string (topologyresolver_t *self, const char *separator)
{
    return "NA";
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

    // TODO: replace with zhash lookup one whoami is done
    fty_proto_t *msg = fty_proto_new(FTY_PROTO_ASSET);

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
            zlistx_add_end (list, (void *)parent);
        }
    }

    // TODO: remove when lookup is done
    fty_proto_destroy (&msg);
    return list;
}
