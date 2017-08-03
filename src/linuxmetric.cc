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
#include <sys/statvfs.h>
#include <cxxtools/directory.h>

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

static zlistx_t *
s_meminfo (void)
{
    zlistx_t *meminfo = zlistx_new ();

    std::string line_total = s_getline_by_name ("/proc/meminfo", "MemTotal:");
    double memory_total = s_get_field (line_total, 2);

    linuxmetric_t *memory_total_info = linuxmetric_new ();
    memory_total_info->type = "total.memory";
    memory_total_info->value = memory_total;
    memory_total_info->unit = "kB";
    zlistx_add_end (meminfo, memory_total_info);

    std::string line_free = s_getline_by_name ("/proc/meminfo", "MemFree:");
    double memory_free = s_get_field (line_free, 2);
    double memory_used = memory_total - memory_free;

    linuxmetric_t *memory_used_info = linuxmetric_new ();
    memory_used_info->type = "used.memory";
    memory_used_info->value = memory_used;
    memory_used_info->unit = "kB";
    zlistx_add_end (meminfo, memory_used_info);

    linuxmetric_t *memory_usage_info = linuxmetric_new ();
    memory_usage_info->type = "usage.memory";
    memory_usage_info->value = 100 * (memory_used / memory_total);
    memory_usage_info->unit = "%";
    zlistx_add_end (meminfo, memory_usage_info);

    return meminfo;
}

static zlistx_t *
s_sdcard_info (void)
{
    zlistx_t *sdcard_info = zlistx_new ();

    struct statvfs buf;
    statvfs ("/var", &buf);
    int to_MB = 1024 * 1024;

    double sdcard_total = buf.f_blocks * buf.f_frsize;
    linuxmetric_t *sdcard_total_info = linuxmetric_new ();
    sdcard_total_info->type = "total.sd-card";
    sdcard_total_info->value = sdcard_total / to_MB;
    sdcard_total_info->unit = "MB";
    zlistx_add_end (sdcard_info, sdcard_total_info);

    double sdcard_used = sdcard_total - buf.f_bsize * buf.f_bfree;
    linuxmetric_t *sdcard_used_info = linuxmetric_new ();
    sdcard_used_info->type = "used.sd-card";
    sdcard_used_info->value = sdcard_used / to_MB;
    sdcard_used_info->unit = "MB";
    zlistx_add_end (sdcard_info, sdcard_used_info);

    linuxmetric_t *sdcard_usage_info = linuxmetric_new ();
    sdcard_usage_info->type = "usage.sd-card";
    sdcard_usage_info->value = 100 * (sdcard_used / sdcard_total);
    sdcard_usage_info->unit = "%";
    zlistx_add_end (sdcard_info, sdcard_usage_info);

    return sdcard_info;
}

static zlistx_t *
s_flash_info (void)
{
    zlistx_t *flash_info = zlistx_new ();

    struct statvfs buf;
    statvfs ("/", &buf);
    int to_MB = 1024 * 1024;

    double flash_total = buf.f_blocks * buf.f_frsize;
    linuxmetric_t *flash_total_info = linuxmetric_new ();
    flash_total_info->type = "total.flash";
    flash_total_info->value = flash_total / to_MB;
    flash_total_info->unit = "MB";
    zlistx_add_end (flash_info, flash_total_info);

    double flash_used = flash_total - buf.f_bsize * buf.f_bfree;
    linuxmetric_t *flash_used_info = linuxmetric_new ();
    flash_used_info->type = "used.flash";
    flash_used_info->value = flash_used / to_MB;
    flash_used_info->unit = "MB";
    zlistx_add_end (flash_info, flash_used_info);

    //df -h computes "/" usage from f_bavail, let's do the same
    double flash_used_nonroot = flash_total - buf.f_bsize * buf.f_bavail;
    linuxmetric_t *flash_usage_info = linuxmetric_new ();
    flash_usage_info->type = "usage.flash";
    flash_usage_info->value = 100 * (flash_used_nonroot / flash_total);
    flash_usage_info->unit = "%";
    zlistx_add_end (flash_info, flash_usage_info);

    return flash_info;
}

static bool
is_interface_online (const char *interface)
{
    char *interface_dir = zsys_sprintf ("/sys/class/net/%s/", interface);

    // does the interface exist?
    const std::string dir(interface_dir);
    if (!cxxtools::Directory::exists (dir))
        return false;

    // is the interface up?
    char *interface_state = zsys_sprintf ("%s/operstate", interface_dir);
    std::string state = s_getline_by_number (interface_state, 1);
    zstr_free (&interface_state);
    zstr_free (&interface_dir);
    return (state == "up");
}

static zlistx_t *
    s_network_usage
    (const char *interface,
     const char *direction,
     int interval,
     std::map<std::string,double> &network_history)
{
    char *key = zsys_sprintf ("%s_%s", direction, interface);
    std::string skey(key);
    auto pair_last = network_history.find (skey);
    double value_last = 0;
    if (pair_last != network_history.end ()) {
        value_last = pair_last->second;
        zsys_debug ("%s:key found, value %lf", key, value_last);
    }

    zlistx_t *network_usage_info = zlistx_new ();

    char *path = zsys_sprintf ("/sys/class/net/%s/statistics/%s_bytes", interface, direction);
    std::string line = s_getline_by_number (path, 1);
    double bytes = s_get_field (line, 1);

    linuxmetric_t *bandwidth_info = linuxmetric_new ();
    char *bandwidth_type = zsys_sprintf ("%s_bandwidth.%s", direction, interface);
    bandwidth_info->type = strdup (bandwidth_type);
    bandwidth_info->value = (bytes - value_last) / interval;
    bandwidth_info->unit = "Bps";
    zlistx_add_end (network_usage_info, bandwidth_info);

    linuxmetric_t *bytes_info = linuxmetric_new ();
    char *bytes_type = zsys_sprintf ("%s_bytes.%s", direction, interface);
    bytes_info->type = strdup (bytes_type);
    bytes_info->value = bytes;
    bytes_info->unit = "B";
    zlistx_add_end (network_usage_info, bytes_info);

    network_history[skey] = bytes;

    zstr_free (&bytes_type);
    zstr_free (&bandwidth_type);
    zstr_free (&path);
    zstr_free (&key);

    return network_usage_info;
}

static linuxmetric_t *
    s_network_error_ratio
    (const char *interface,
     const char *direction,
     std::map<std::string,double> &network_history)
{
    char *key = zsys_sprintf ("%s_%s_errors", direction, interface);
    std::string skey(key);
    auto pair_last = network_history.find (skey);
    double value_last = 0;
    if (pair_last != network_history.end ()) {
        value_last = pair_last->second;
        zsys_debug ("%s:key found, value %lf", key, value_last);
    }

    char *errors_path = zsys_sprintf ("/sys/class/net/%s/statistics/%s_errors", interface, direction);
    std::string errors_line = s_getline_by_number (errors_path, 1);
    double errors = s_get_field (errors_line, 1);

    char *packets_path = zsys_sprintf ("/sys/class/net/%s/statistics/%s_packets", interface, direction);
    std::string packets_line = s_getline_by_number (packets_path, 1);
    double packets = s_get_field (packets_line, 1);

    linuxmetric_t *error_info = linuxmetric_new ();
    char *error_type = zsys_sprintf ("%s_error_ratio.%s", direction, interface);
    error_info->type = strdup (error_type);
    error_info->value = 100 * (errors - value_last) / packets;
    error_info->unit = "%";

    network_history[skey] = errors;

    zstr_free (&error_type);
    zstr_free (&packets_path);
    zstr_free (&errors_path);
    zstr_free (&key);
    return error_info;
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
linuxmetric_get_all (int interval, std::map<std::string,double> &network_history)
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

    zlistx_t *meminfo = s_meminfo ();
    linuxmetric_t *mem_metric = (linuxmetric_t *) zlistx_first (meminfo);
    while (mem_metric) {
        zlistx_add_end (info, mem_metric);
        mem_metric = (linuxmetric_t *) zlistx_next (meminfo);
    }
    zlistx_destroy (&meminfo);

    zlistx_t *sdcard_info = s_sdcard_info ();
    linuxmetric_t *sdcard_metric = (linuxmetric_t *) zlistx_first (sdcard_info);
    while (sdcard_metric) {
        zlistx_add_end (info, sdcard_metric);
        sdcard_metric = (linuxmetric_t *) zlistx_next (sdcard_info);
    }
    zlistx_destroy (&sdcard_info);

    zlistx_t *flash_info = s_flash_info ();
    linuxmetric_t *flash_metric = (linuxmetric_t *) zlistx_first (flash_info);
    while (flash_metric) {
        zlistx_add_end (info, flash_metric);
        flash_metric = (linuxmetric_t *) zlistx_next (flash_info);
    }
    zlistx_destroy (&flash_info);

    // loop over all network interfaces
    cxxtools::Directory dir("/sys/class/net/");

    for (cxxtools::DirectoryIterator it = dir.begin (true); it != dir.end (); ++it)
    {
        std::string iface = *it;
        if (is_interface_online (iface.c_str ())) {
            zlistx_t *rx = s_network_usage (iface.c_str (), "rx", interval, network_history);
            linuxmetric_t *network_usage_metric = (linuxmetric_t *) zlistx_first (rx);
            while (network_usage_metric) {
                zlistx_add_end (info, network_usage_metric);
                network_usage_metric = (linuxmetric_t *) zlistx_next (rx);
            }
            zlistx_destroy (&rx);

            zlistx_t *tx = s_network_usage (iface.c_str (), "tx", interval, network_history);
            network_usage_metric = (linuxmetric_t *) zlistx_first (tx);
            while (network_usage_metric) {
                zlistx_add_end (info, network_usage_metric);
                network_usage_metric = (linuxmetric_t *) zlistx_next (tx);
            }
            zlistx_destroy (&tx);

	    linuxmetric_t *rx_error = s_network_error_ratio (iface.c_str (), "rx", network_history);
            if (rx_error != NULL)
                zlistx_add_end (info, rx_error);

            linuxmetric_t *tx_error = s_network_error_ratio (iface.c_str (), "tx", network_history);
            if (tx_error != NULL)
                zlistx_add_end (info, tx_error);
        }
    }

    return info;
}
