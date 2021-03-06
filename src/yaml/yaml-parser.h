/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2019 VMware, Inc.
 */

#pragma once

#include <yaml.h>

typedef enum ConfType {
        CONF_TYPE_NETWORK,
        CONF_TYPE_WIFI,
        CONF_TYPE_ROUTE,
        _CONF_TYPE_MAX,
        _CONF_TYPE_INVALID = -1,
} ConfType;

typedef struct ParserTable {
        const char *key;
        ConfType type;
        int (*parser) (const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
        const size_t offset;
} ParserTable;

int parse_yaml_bool(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_uint32(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_mac_address(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_string(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_auth_key_management_type(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_auth_eap_method(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_dhcp_client_identifier(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_dhcp_type(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_addresses(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_routes(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_address(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
int parse_yaml_link_local_type(const char *key, const char *value, void *data, void *userdata, yaml_document_t *doc, yaml_node_t *node);
