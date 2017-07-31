/*  =========================================================================
    linuxmetric - Class for finding out Linux system info

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
    linuxmetric - Class for finding out Linux system info
@discuss
@end
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>

#include "fty_info_classes.h"


///////////////////////////////////////////
// Static functions which parse /proc files
//////////////////////////////////////////

// Get n-th field of the string (counted from 1), which can be parsed as double
static double
s_get_field (std::string line, int index)
{
        std::istringstream stream (line);
        int i = 1;
        std::string field;
        stream.exceptions ( std::istringstream::failbit | std::istringstream::badbit );
        try {
            while (!stream.eof () && i <= index) {
                stream >> field;
                i++;
            }
            return std::stod (field);
        }
        catch (std::istringstream::failure e) {
            zsys_error ("Error while parsing string %s", line.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
        catch (std::invalid_argument& e) {
            zsys_error ("Requested field %s is not a double", field.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
        catch (std::out_of_range& e) {
            zsys_error ("Requested field %s is out of range", field.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
}


// Get line number n (counted from 1)
static std::string
s_getline_by_number (const char *filename, int index)
{
    std::ifstream file (filename, std::ifstream::in);
    if (!file) {
        zsys_error ("Could not open '%s'", filename);
        return "";
    }

    file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    try {
        int i = 1;
        std::string line;
        // ignore first (n-1) lines
        while (!file.eof () && i < index) {
            file.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
            i++;
        }
        std::getline (file, line);
        return line;
    }
    catch (std::ifstream::failure e) {
        zsys_error ("Error while reading file %s", filename);
        return "";
    }
}

// Get line starting with <name>
static std::string
s_getline_by_name (const char *filename, const char *name)
{
    std::ifstream file (filename, std::ifstream::in);
    if (!file) {
        zsys_error ("Could not open '%s'", filename);
        return "";
    }

    file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    try {
        std::string line;
        do {
            std::getline (file, line);
        } while (!file.eof () && strncmp (line.c_str (), name, strlen (name)) != 0);
        return line;
    }
    catch (std::ifstream::failure e) {
        zsys_error ("Error while reading file %s", filename);
        return "";
    }
}


////////////////////////////////////////////////////////////
// Static functions which get metrics values
// All magical constants can be found in /proc and /sys documentation.
////////////////////////////////////////////////////////////

static linuxmetric_t *
s_uptime (void)
{
    std::string line = s_getline_by_number ("/proc/uptime", 1);
    double uptime = s_get_field (line, 1);

    linuxmetric_t *uptime_info = linuxmetric_new ();
    uptime_info->type = "uptime";
    uptime_info->value = uptime;
    uptime_info->unit = "sec";

    return uptime_info;
}

static linuxmetric_t *
s_cpu_usage (void)
{
    std::string line_cpu = s_getline_by_name ("/proc/stat", "cpu");
    double user = s_get_field (line_cpu, 2);
    double nice = s_get_field (line_cpu, 3);
    double system = s_get_field (line_cpu, 4);
    double idle = s_get_field (line_cpu, 5);
    double iowait = s_get_field (line_cpu, 6);
    double irq = s_get_field (line_cpu, 7);
    double softirq = s_get_field (line_cpu, 8);
    double steal = s_get_field (line_cpu, 9);

    linuxmetric_t *cpu_usage_info = linuxmetric_new ();
    cpu_usage_info->type = "usage.cpu";
    cpu_usage_info->value = 100 - 100*((idle + iowait)/(user + nice + system + idle + iowait + irq + softirq + steal));
    cpu_usage_info->unit = "%";

    return cpu_usage_info;
}

static linuxmetric_t *
s_cpu_temperature (void)
{
    std::string line = s_getline_by_number ("/sys/class/thermal/thermal_zone0/temp", 1);
    if (!line.empty ()) {
        double temperature = s_get_field (line, 1);

        linuxmetric_t *cpu_temperature_info = linuxmetric_new ();
        cpu_temperature_info->type = "temperature.cpu";
        cpu_temperature_info->value = temperature / 1000;
        cpu_temperature_info->unit = "C";
        return cpu_temperature_info;
    }
    return NULL;
}

//  --------------------------------------------------------------------------
//  Create a new linuxmetric

linuxmetric_t *
linuxmetric_new (void)
{
    linuxmetric_t *self = (linuxmetric_t *) zmalloc (sizeof (linuxmetric_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the linuxmetric

void
linuxmetric_destroy (linuxmetric_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        linuxmetric_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//--------------------------------------------------------------------------
//// Create zlistx containing all Linux system info

zlistx_t *
linuxmetric_get_all (void)
{
    zlistx_t *info = zlistx_new ();
    zlistx_set_destructor (info, (void (*)(void**)) linuxmetric_destroy);
    linuxmetric_t *uptime = s_uptime ();
    zlistx_add_end (info, uptime);
    linuxmetric_t *cpu_usage = s_cpu_usage ();
    zlistx_add_end (info, cpu_usage);
    linuxmetric_t *cpu_temperature = s_cpu_temperature ();
    if (cpu_temperature != NULL)
        zlistx_add_end (info, cpu_temperature);
    return info;
}
