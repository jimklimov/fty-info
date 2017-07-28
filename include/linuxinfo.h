/*  =========================================================================
    linuxinfo - Class for finding out Linux system info

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

#ifndef LINUXINFO_H_INCLUDED
#define LINUXINFO_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

struct _linuxinfo_t {
    const char *type;
    double value;
    const char *unit;
};


//  @interface
//  Create a new linuxinfo
FTY_INFO_EXPORT linuxinfo_t *
    linuxinfo_new (void);

//  Destroy the linuxinfo
FTY_INFO_EXPORT void
    linuxinfo_destroy (linuxinfo_t **self_p);

// Create zlistx containing all Linux system info
FTY_INFO_EXPORT zlistx_t *
    linuxinfo_get_all (void);

//  @end

#ifdef __cplusplus
}
#endif

#endif
