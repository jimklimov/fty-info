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

}

//  --------------------------------------------------------------------------
//  Return topology as string of friedly names (or NULL if incomplete)
const char *
topologyresolver_to_string (topologyresolver_t *self, const char *separator)
{
    return NULL;
}

//  --------------------------------------------------------------------------
//  Return zlist of inames starting with asset up to DC
//  Empty list is returned if the topology is incomplete yet
zlistx_t *
topologyresolver_to_list (topologyresolver_t *self)
{
    return zlistx_new();
}
