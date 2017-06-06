/*  =========================================================================
    fty-info - Agent which returns rack controller information

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

#ifndef FTY_INFO_H_H_INCLUDED
#define FTY_INFO_H_H_INCLUDED

//  Include the project library file
#include "fty_info_library.h"

//  Add your own public definitions here, if you need them
#define FTY_INFO_AGENT "fty-info"
#define FTY_INFO_CMD "INFO"

#define INFO_UUID      "uuid"
#define INFO_HOSTNAME  "hostname"
#define INFO_NAME      "name"
#define INFO_NAME_URI      "name-uri"
#define INFO_VENDOR    "manufacturer"
#define INFO_SERIAL    "serialNumber"
#define INFO_MODEL     "model"
#define INFO_PART_NUMBER    "partNumber"
#define INFO_LOCATION  "location"
#define INFO_PARENT_URI  "parent-uri"
#define INFO_VERSION   "version"
#define INFO_REST_PATH "path"
#define INFO_REST_PORT "port"
#define INFO_PROTOCOL_FORMAT "protocol-format"
#define INFO_TYPE      "type"
#define INFO_TXTVERS   "txtvers"

#endif
