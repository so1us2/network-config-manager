/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2019 VMware, Inc.
 */

#include <assert.h>
#include <net/if.h>
#include <linux/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "alloc-util.h"
#include "file-util.h"
#include "macros.h"
#include "log.h"
#include "netlink-message.h"
#include "netlink.h"
#include "network-link.h"
#include "network-util.h"
#include "parse-util.h"
#include "string-util.h"

static const char* const link_operstates[] = {
        [IF_OPER_UNKNOWN]        = "unknown",
        [IF_OPER_NOTPRESENT]     = "not-present",
        [IF_OPER_DOWN]           = "down",
        [IF_OPER_LOWERLAYERDOWN] = "lower-layerdown",
        [IF_OPER_TESTING]        = "testing",
        [IF_OPER_DORMANT]        = "dormant",
        [IF_OPER_UP]             = "up",
};

const char *link_operstates_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(link_operstates))
                return NULL;

        return link_operstates[id];
}

static const char* const link_states[_LINK_STATE_MAX] = {
        [LINK_STATE_DOWN] = "down",
        [LINK_STATE_UP]   = "up",
};

const char *link_state_to_name(int id) {
        if (id < 0)
                return "n/a";

        if ((size_t) id >= ELEMENTSOF(link_states))
                return "n/a";

        return link_states[id];
}

int link_name_to_state(char *name) {
        int i;

        assert(name);

        for (i = LINK_STATE_DOWN; i < (int) ELEMENTSOF(link_states); i++)
                if (string_equal_fold(name, link_states[i]))
                        return i;

        return _LINK_STATE_INVALID;
}

static int links_new(Links **ret) {
        Links *h = NULL;

        h = new0(Links, 1);
        if (!h)
                return log_oom();

        *ret = h;

        return 0;
}

static int link_new(Link **ret) {
        Link *link = NULL;

        link = new(Link, 1);
        if (!link)
                return log_oom();

        *ret = link;

        return 0;
}

static int link_add(Links **h, Link *link) {
        int r;

        assert(h);
        assert(link);

        if (!*h) {
                r = links_new(h);
                if (r < 0)
                        return r;
        }

        (*h)->links = g_list_append((*h)->links, link);

        return 0;
}

static int fill_one_link_info(struct nlmsghdr *h, size_t len, Link **ret) {
        _auto_cleanup_ struct rtattr **rta_tb = NULL;
        _auto_cleanup_ Link *n = NULL;
        struct ifinfomsg *iface;
        struct nlmsghdr *p;
        int r, l;

        r = link_new(&n);
        if (r < 0)
                return r;
        p = h;

        iface = NLMSG_DATA(p);
        l = p->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

        rta_tb = new0(struct rtattr *, IFLA_MAX + 1);
        if (!rta_tb)
                return log_oom();

        r = rtnl_message_parse_rtattr(rta_tb, IFLA_MAX, IFLA_RTA(iface), l);
        if (r < 0)
                return r;

        n->ifindex = iface->ifi_index;
        n->iftype = iface->ifi_type;

        if (rta_tb[IFLA_IFNAME])
                memcpy(n->name, rtnl_message_read_attribute_string(rta_tb[IFLA_IFNAME]), IFNAMSIZ);

        if (rta_tb[IFLA_MTU]) {
                n->mtu = rtnl_message_read_attribute_u32(rta_tb[IFLA_MTU]);
                n->contains_mtu = true;
        }

        if (rta_tb[IFLA_OPERSTATE])
                n->operstate = rtnl_message_read_attribute_u8(rta_tb[IFLA_OPERSTATE]);

        if (rta_tb[IFLA_ADDRESS]) {
                rtnl_message_read_attribute_ether_address(rta_tb[IFLA_ADDRESS], &n->mac_address);
                n->contains_mac_address = true;
        }

        *ret = steal_pointer(n);

        return 0;
}

static int acquire_one_link_info(int s, int ifindex, Link **ret) {
        _auto_cleanup_ IPlinkMessage *m = NULL;
        _auto_cleanup_ Link *link = NULL;
        struct nlmsghdr *reply = NULL;
        int r;

        assert(s);
        assert(ifindex > 0);
        assert(ret);

        r = ip_link_message_new(RTM_GETLINK, AF_UNSPEC, ifindex, &m);
        if (r < 0)
                return r;

        r = rtnl_send_message(s, &m->hdr);
        if (r < 0)
                return r;

        r = rtnl_receive_message(s, m->buf, sizeof(m->buf), 0);
        if (r < 0)
                return r;

        reply = (struct nlmsghdr *) m->buf;
        r = fill_one_link_info(reply, r, &link);
        if (r < 0)
                return r;

        *ret = steal_pointer(link);

        return 0;
}

int link_get_one_link(const char *ifname, Link **ret) {
        _auto_cleanup_close_ int s = -1;
        int r, c;

        assert(ifname);
        assert(ret);

        r = rtnl_socket_open(0, &s);
        if (r < 0)
                return r;

        r = (int) if_nametoindex(ifname);
        if (r <= 0)
                return -errno;

        c = acquire_one_link_info(s, r, ret);
        if (c < 0)
                return c;

        return 0;
}

static int fill_link_info(Links **links, struct nlmsghdr *h, size_t len) {
        _auto_cleanup_ Link *n = NULL;
        struct ifinfomsg *iface;
        struct nlmsghdr *p;
        int r, l;

        for (p = h; NLMSG_OK(p, len); p = NLMSG_NEXT(p, len)) {
                _auto_cleanup_ struct rtattr **rta_tb = NULL;

                iface = NLMSG_DATA(p);
                l = p->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

                rta_tb = new0(struct rtattr *, IFLA_MAX + 1);
                if (!rta_tb)
                        return log_oom();

                r = rtnl_message_parse_rtattr(rta_tb, IFLA_MAX, IFLA_RTA(iface), l);
                if (r < 0)
                        return r;

                r = link_new(&n);
                if (r < 0)
                        return r;

                n->ifindex = iface->ifi_index;
                n->iftype = iface->ifi_type;

                if (rta_tb[IFLA_IFNAME])
                        memcpy(n->name, rtnl_message_read_attribute_string(rta_tb[IFLA_IFNAME]), IFNAMSIZ);

                if (rta_tb[IFLA_MTU]) {
                        n->mtu = rtnl_message_read_attribute_u32(rta_tb[IFLA_MTU]);
                        n->contains_mtu = true;
                }

                if (rta_tb[IFLA_OPERSTATE])
                        n->operstate = rtnl_message_read_attribute_u8(rta_tb[IFLA_OPERSTATE]);

                if (rta_tb[IFLA_ADDRESS]) {
                        rtnl_message_read_attribute_ether_address(rta_tb[IFLA_ADDRESS], &n->mac_address);
                        n->contains_mac_address = true;
                }

                r = link_add(links, n);
                if (r < 0)
                        return r;

                n = NULL;
        }

        return 0;
}

static int acquire_link_info(int s, Links **ret) {
        _auto_cleanup_ IPlinkMessage *m = NULL;
        _auto_cleanup_ Links *links = NULL;
        struct nlmsghdr *reply = NULL;
        int r;

        assert(s);
        assert(ret);

        r = ip_link_message_new(RTM_GETLINK, AF_UNSPEC, 0, &m);
        if (r < 0)
                return r;

        r = rtnl_message_request_dump(&m->hdr, true);
        if (r < 0)
                return r;

        r = rtnl_send_message(s, &m->hdr);
        if (r < 0)
                return r;

        r = rtnl_receive_message(s, m->buf, sizeof(m->buf), 0);
        if (r < 0)
                 return r;

        reply = (struct nlmsghdr *) m->buf;
        for(; r > 0;) {
                fill_link_info(&links, reply, r);

                r = rtnl_receive_message(s, m->buf, sizeof(m->buf), 0);
                if (r < 0)
                        return -errno;
                if (r == 0)
                         break;
         }

        *ret = steal_pointer(links);

        return 0;
}

int link_get_links(Links **ret) {
       _auto_cleanup_close_ int s = -1;
        int r;

        r = rtnl_socket_open(0, &s);
        if (r < 0)
                return r;

        r = acquire_link_info(s, ret);
        if (r < 0)
                return r;

        return 0;
}

int link_update_mtu(const IfNameIndex *ifnameidx, uint32_t mtu) {
      _auto_cleanup_ IPlinkMessage *m = NULL;
      _auto_cleanup_close_ int s = -1;
      int r;

      assert(mtu > 0);
      assert(ifnameidx);

      r = ip_link_message_new(RTM_SETLINK, AF_UNSPEC, ifnameidx->ifindex, &m);
      if (r < 0)
                return r;

      r = rtnl_message_add_attribute_uint32(&m->hdr, IFLA_MTU, mtu);
      if (r < 0)
                return r;

      r = rtnl_socket_open(0, &s);
      if (r < 0)
              return r;

      return netlink_call(s, &m->hdr, m->buf, sizeof(m->buf));
}

int link_set_mac_address(const IfNameIndex *ifnameidx, const char *mac_address) {
        _auto_cleanup_ IPlinkMessage *m = NULL;
        _auto_cleanup_close_ int s = -1;
        int r;

        assert(mac_address);
        assert(ifnameidx);

        r = ip_link_message_new(RTM_SETLINK, AF_UNSPEC, ifnameidx->ifindex, &m);
        if (r < 0)
                return r;

        r = rtnl_message_add_attribute(&m->hdr, IFLA_ADDRESS, ether_aton(mac_address), ETH_ALEN);
        if (r < 0)
                return r;

        r = rtnl_socket_open(0, &s);
        if (r < 0)
                return r;

        return netlink_call(s, &m->hdr, m->buf, sizeof(m->buf));
}

int link_set_state(const IfNameIndex *ifnameidx, LinkState state) {
        _auto_cleanup_ IPlinkMessage *m = NULL;
        _auto_cleanup_ char *operstate = NULL;
        _auto_cleanup_close_ int s = -1;
        int r;

        assert(ifnameidx);

        r = link_get_operstate(ifnameidx->ifname, &operstate);
        if (r < 0) {
                log_warning("Failed to get link operstate: %s\n", ifnameidx->ifname);
                return r;
        }

        if ((int) state == link_name_to_state(operstate))
                return 0;

        r = ip_link_message_new(RTM_SETLINK, AF_UNSPEC, ifnameidx->ifindex, &m);
        if (r < 0)
                return r;

        switch (state) {
        case LINK_STATE_UP:
                SET_FLAG(m->ifi.ifi_change, IFF_UP, state);
                SET_FLAG(m->ifi.ifi_flags, IFF_UP, state);
                break;
        case LINK_STATE_DOWN:
                SET_FLAG(m->ifi.ifi_change, IFF_UP, IFF_UP);
                SET_FLAG(m->ifi.ifi_flags, ~IFF_UP, ~IFF_UP);
                break;
        default:
                assert(0);
        }

        r = rtnl_socket_open(0, &s);
        if (r < 0)
                return r;

        return netlink_call(s, &m->hdr, m->buf, sizeof(m->buf));
}

int link_read_sysfs_attribute(const char *ifname, const char *attribute, char **ret) {
        _auto_cleanup_ char *line = NULL, *path = NULL;
        int r;

        assert(ifname);
        assert(attribute);

        path = g_build_path("/", "/sys/class/net", ifname, attribute, NULL);
        if (!path)
                return log_oom();

        r = read_one_line(path, &line);
        if (r < 0)
                return r;

        truncate_newline(line);

        *ret = steal_pointer(line);

        return 0;
}

int link_get_mtu(const char *ifname, uint32_t *mtu) {
        _auto_cleanup_ char *s = NULL;
        int r, k;

        (void) link_read_sysfs_attribute(ifname, "mtu", &s);

        r = parse_integer(s, &k);
        if (r < 0)
                return r;

        *mtu = k;

        return 0;
}

int link_get_mac_address(const char *ifname, char **mac) {
        return link_read_sysfs_attribute(ifname, "address", mac);
}

int link_get_operstate(const char *ifname, char **operstate) {
        return link_read_sysfs_attribute(ifname, "operstate", operstate);
}
