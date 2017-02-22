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

#include "fty_info_classes.h"

//  Structure of our class

struct _fty_info_server_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new fty_info_server

fty_info_server_t *
fty_info_server_new (void)
{
    fty_info_server_t *self = (fty_info_server_t *) zmalloc (sizeof (fty_info_server_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fty_info_server

void
fty_info_server_destroy (fty_info_server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fty_info_server_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_info_server_test (bool verbose)
{
    printf (" * fty_info_server: ");

    //  @selftest
    //  Simple create/destroy test
    fty_info_server_t *self = fty_info_server_new ();
    assert (self);
    fty_info_server_destroy (&self);
    //  @end
    printf ("OK\n");
}
