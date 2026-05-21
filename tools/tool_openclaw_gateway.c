/**
 * @file tool_openclaw_gateway.c
 * @brief MCP tools to read/update OpenClaw ACP gateway host, token, and port (tal_kv)
 * @version 1.0
 * @date 2026-04-16
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "tool_openclaw_gateway.h"

#include "openclaw_gateway_cfg.h"
#include "acp_client.h"
#include "ai_mcp_server.h"

#include "tal_log.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief Read string MCP property
 * @param[in] props Property list
 * @param[in] name Property name
 * @return Pointer to string or NULL
 */
static const char *__get_str_prop(const MCP_PROPERTY_LIST_T *props, const char *name)
{
    const MCP_PROPERTY_T *p = ai_mcp_property_list_find(props, name);
    if (p && p->type == MCP_PROPERTY_TYPE_STRING && p->default_val.str_val) {
        return p->default_val.str_val;
    }
    return NULL;
}

/**
 * @brief Read integer MCP property
 * @param[in] props Property list
 * @param[in] name Property name
 * @param[out] out Output value
 * @return true if property exists as integer
 */
static bool __get_int_prop(const MCP_PROPERTY_LIST_T *props, const char *name, int *out)
{
    const MCP_PROPERTY_T *p = ai_mcp_property_list_find(props, name);
    if (p && p->type == MCP_PROPERTY_TYPE_INTEGER) {
        *out = p->default_val.int_val;
        return true;
    }
    return false;
}

/**
 * @brief Append masked token preview to buffer (never log or return full secret)
 * @param[in] token Raw token
 * @param[out] out Output buffer
 * @param[in] out_size Buffer size
 * @return none
 */
static void __mask_token_line(const char *token, char *out, size_t out_size)
{
    size_t n;

    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!token || token[0] == '\0') {
        (void)snprintf(out, out_size, "token=(empty)");
        return;
    }
    n = strlen(token);
    if (n <= 6) {
        (void)snprintf(out, out_size, "token=(set, len=%u)", (unsigned)n);
    } else {
        (void)snprintf(out, out_size, "token_prefix=%.4s… len=%u", token, (unsigned)n);
    }
}

/* ---------------------------------------------------------------------------
 * MCP callbacks
 * --------------------------------------------------------------------------- */

/**
 * @brief MCP tool: openclaw_gateway_get
 */
static OPERATE_RET __tool_openclaw_gateway_get(const MCP_PROPERTY_LIST_T *properties,
                                                MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    char     host[OPENCLAW_GATEWAY_HOST_MAX];
    char     tok_line[96];
    uint16_t port;

    (void)properties;
    (void)user_data;

    openclaw_gateway_cfg_get_host(host, sizeof(host));
    {
        char tok[OPENCLAW_GATEWAY_TOKEN_MAX];
        openclaw_gateway_cfg_get_token(tok, sizeof(tok));
        __mask_token_line(tok, tok_line, sizeof(tok_line));
    }
    port = openclaw_gateway_cfg_get_port();

    {
        char buf[384];
        (void)snprintf(buf, sizeof(buf),
                       "OpenClaw ACP gateway: host=%s port=%u %s connected=%d",
                       host, (unsigned)port, tok_line,
                       acp_client_is_connected() ? 1 : 0);
        ai_mcp_return_value_set_str(ret_val, buf);
    }
    return OPRT_OK;
}

/**
 * @brief MCP tool: openclaw_gateway_set
 */
static OPERATE_RET __tool_openclaw_gateway_set(const MCP_PROPERTY_LIST_T *properties,
                                               MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *host  = __get_str_prop(properties, "host");
    const char *token = __get_str_prop(properties, "token");
    int         port_i = 0;
    bool        have_port = __get_int_prop(properties, "port", &port_i);
    OPERATE_RET rt;

    (void)user_data;

    if (!have_port) {
        port_i = 0;
    }

    rt = openclaw_gateway_cfg_apply_patch(host, token, (int32_t)port_i);
    if (rt == OPRT_INVALID_PARM) {
        ai_mcp_return_value_set_str(ret_val,
            "Error: provide at least one of: non-empty host, non-empty token, or port in 1..65535 "
            "(use 0 to leave port unchanged).");
        return OPRT_INVALID_PARM;
    }
    if (rt != OPRT_OK) {
        ai_mcp_return_value_set_str(ret_val, "Error: failed to persist gateway settings (KV).");
        return rt;
    }

    acp_client_request_reconnect();

    ai_mcp_return_value_set_str(ret_val,
        "OK: OpenClaw gateway settings saved and ACP reconnect requested. "
        "Use openclaw_gateway_get to verify host/port; connection may take a few seconds.");
    return OPRT_OK;
}

/**
 * @brief Add one MCP property from a definition macro to a tool
 * @param[in] tool Target tool
 * @param[in] prop_def Property definition pointer from MCP_PROP_* macro
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __tool_add_prop(MCP_TOOL_T *tool, MCP_PROPERTY_DEF_T *prop_def)
{
    OPERATE_RET rt;
    MCP_PROPERTY_T *prop;

    if (tool == NULL || prop_def == NULL) {
        return OPRT_INVALID_PARM;
    }

    prop = ai_mcp_property_create_from_def(prop_def);
    if (prop == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    rt = ai_mcp_tool_add_property(tool, prop);
    if (rt != OPRT_OK) {
        ai_mcp_property_destroy(prop);
    }
    return rt;
}

/**
 * @brief Register openclaw_gateway_set without variadic MCP_PROP macros
 * @return OPRT_OK on success
 * @note AI_MCP_TOOL_ADD splits on commas inside MCP_PROP_* when multiple props are passed.
 */
STATIC OPERATE_RET __register_gateway_set_tool(void)
{
    OPERATE_RET rt;
    MCP_TOOL_T *tool;

    tool = ai_mcp_tool_create(
        "openclaw_gateway_set",
        "Update OpenClaw ACP gateway connection (persists in device KV; survives reboot). "
        "Pass any subset: host (hostname or IPv4), token (gateway auth token), port 1-65535; use 0 to keep current port. "
        "At least one field must be set. After save, the ACP client reconnects automatically. "
        "Typical use: user configures gateway via Feishu/Telegram/Discord by telling you the new LAN IP or token.",
        __tool_openclaw_gateway_set,
        NULL);
    if (tool == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    rt = __tool_add_prop(tool, MCP_PROP_STR_DEF("host", "Gateway hostname or IPv4; empty leaves host unchanged", ""));
    if (rt != OPRT_OK) {
        ai_mcp_tool_destroy(tool);
        return rt;
    }

    rt = __tool_add_prop(tool, MCP_PROP_STR_DEF("token", "Gateway auth token; empty leaves token unchanged", ""));
    if (rt != OPRT_OK) {
        ai_mcp_tool_destroy(tool);
        return rt;
    }

    rt = __tool_add_prop(tool, &(MCP_PROPERTY_DEF_T){
        .name = "port",
        .type = MCP_PROPERTY_TYPE_INTEGER,
        .description = "TCP port; 0 leaves port unchanged",
        .has_default = TRUE,
        .default_val.int_val = 0,
        .has_range = FALSE,
    });
    if (rt != OPRT_OK) {
        ai_mcp_tool_destroy(tool);
        return rt;
    }

    rt = ai_mcp_server_add_tool(tool);
    if (rt != OPRT_OK) {
        ai_mcp_tool_destroy(tool);
    }
    return rt;
}

/**
 * @brief Register gateway config MCP tools
 * @return OPRT_OK on success
 */
OPERATE_RET tool_openclaw_gateway_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = AI_MCP_TOOL_ADD(
        "openclaw_gateway_get",
        "Read the OpenClaw ACP WebSocket gateway host, port, and a masked token summary. "
        "Use when the user asks which PC/gateway the device is targeting or before changing settings.",
        __tool_openclaw_gateway_get,
        NULL);
    if (rt != OPRT_OK) {
        PR_ERR("openclaw_gateway_get register failed ret=%d", rt);
        return rt;
    }

    rt = __register_gateway_set_tool();
    if (rt != OPRT_OK) {
        PR_ERR("openclaw_gateway_set register failed ret=%d", rt);
        return rt;
    }

    PR_DEBUG("openclaw_gateway MCP tools registered");
    return OPRT_OK;
}
