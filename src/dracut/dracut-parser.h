/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2019 VMware, Inc.
 */

#pragma once

#include <glib.h>

typedef enum DracutDHCPMode {
        DRACUT_DHCP_MODE_NONE,
        DRACUT_DHCP_MODE_OFF,
        DRACUT_DHCP_MODE_DHCP,
        DRACUT_DHCP_MODE_ON,
        DRACUT_DHCP_MODE_ANY,
        DRACUT_DHCP_MODE_DHCP6,
        DRACUT_DHCP_MODE_AUTO6,
        DRACUT_DHCP_MODE_EITHER6,
        DRACUT_DHCP_MODE_IBFT,
        _DRACUT_DHCP_MODE_MAX,
        _DRACUT_DHCP_MODE_INVALID = -1
} DrakutDHCPMode;

typedef enum DracutConfType {
        DRACUT_CONF_TYPE_IP,
        DRACUT_CONF_TYPE_RD_ROUTE,
        DRACUT_CONF_TYPE_NAMESERVER,
        DRACUT_CONF_TYPE_RD_PEERDNS,
        DRACUT_CONF_TYPE_IFNAME,
        DRACUT_CONF_TYPE_BOND,
        DRACUT_CONF_TYPE_TEAM,
        DRACUT_CONF_TYPE_BRIDGE,
        DRACUT_CONF_TYPE_VLAN,
        _DRACUT_CONF_TYPE_MAX,
        _DRACUT_CONF_TYPE_INVALID = -1
} DracutConfType;

const char *dracut_dhcp_mode_to_name(int id);
int dracut_dhcp_mode_to_mode(const char *name);

const char *dracut_to_networkd_dhcp_mode_to_name(int id);
int dracut_to_networkd_dhcp_name_to_mode(const char *name);

int parse_proc_command_line(const char *cmdline, GHashTable **ret);