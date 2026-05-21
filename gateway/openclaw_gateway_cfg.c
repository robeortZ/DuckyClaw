/**
 * @file openclaw_gateway_cfg.c
 * @brief Runtime OpenClaw gateway configuration in tal_kv
 * @version 1.0
 * @date 2026-04-16
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "openclaw_gateway_cfg.h"

#include "tuya_app_config.h"

#include "tal_kv.h"
#include "tal_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */

#define OCGW_KV_HOST  "ocgw_host"
#define OCGW_KV_TOKEN "ocgw_tok"
#define OCGW_KV_PORT  "ocgw_port"

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief Read string KV into buf; on miss or error leaves buf empty
 * @param[in] key KV key
 * @param[out] buf Output buffer
 * @param[in] buf_size Buffer size
 * @return true if a non-empty string was stored in buf
 */
static bool __kv_get_string(const char *key, char *buf, size_t buf_size)
{
    uint8_t *v    = NULL;
    size_t   len  = 0;
    int      ret;

    if (!buf || buf_size == 0) {
        return false;
    }
    buf[0] = '\0';

    ret = tal_kv_get(key, &v, &len);
    if (ret != 0 || v == NULL || len == 0) {
        if (v) {
            tal_kv_free(v);
        }
        return false;
    }

    {
        size_t copy = len < buf_size - 1 ? len : buf_size - 1;
        memcpy(buf, v, copy);
        buf[copy] = '\0';
    }
    tal_kv_free(v);
    return (buf[0] != '\0');
}

/**
 * @brief Write string value to KV
 * @param[in] key KV key
 * @param[in] val NUL-terminated value
 * @return OPRT_OK on success
 */
static OPERATE_RET __kv_set_string(const char *key, const char *val)
{
    int ret;

    if (!key || !val) {
        return OPRT_INVALID_PARM;
    }
    ret = tal_kv_set(key, (const uint8_t *)val, strlen(val));
    if (ret != 0) {
        PR_ERR("openclaw_gateway_cfg: tal_kv_set %s failed ret=%d", key, ret);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

void openclaw_gateway_cfg_get_host(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    if (__kv_get_string(OCGW_KV_HOST, buf, buf_size)) {
        return;
    }
    (void)snprintf(buf, buf_size, "%s", OPENCLAW_GATEWAY_HOST);
}

void openclaw_gateway_cfg_get_token(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    if (__kv_get_string(OCGW_KV_TOKEN, buf, buf_size)) {
        return;
    }
    (void)snprintf(buf, buf_size, "%s", OPENCLAW_GATEWAY_TOKEN);
}

uint16_t openclaw_gateway_cfg_get_port(void)
{
    char pbuf[16];

    if (__kv_get_string(OCGW_KV_PORT, pbuf, sizeof(pbuf))) {
        int p = atoi(pbuf);
        if (p > 0 && p <= 65535) {
            return (uint16_t)p;
        }
    }
    return (uint16_t)OPENCLAW_GATEWAY_PORT;
}

OPERATE_RET openclaw_gateway_cfg_apply_patch(const char *host, const char *token, int32_t port)
{
    OPERATE_RET rt=OPRT_OK;
    char     hbuf[OPENCLAW_GATEWAY_HOST_MAX];
    char     tbuf[OPENCLAW_GATEWAY_TOKEN_MAX];
    char     pstr[12];
    uint16_t po;
    bool     have_host  = (host != NULL && host[0] != '\0');
    bool     have_token = (token != NULL && token[0] != '\0');
    bool     have_port  = (port > 0 && port <= 65535);

    if (!have_host && !have_token && !have_port) {
        return OPRT_INVALID_PARM;
    }

    openclaw_gateway_cfg_get_host(hbuf, sizeof(hbuf));
    openclaw_gateway_cfg_get_token(tbuf, sizeof(tbuf));
    po = openclaw_gateway_cfg_get_port();

    if (have_host) {
        (void)snprintf(hbuf, sizeof(hbuf), "%s", host);
    }
    if (have_token) {
        (void)snprintf(tbuf, sizeof(tbuf), "%s", token);
    }
    if (have_port) {
        po = (uint16_t)port;
    }

    TUYA_CALL_ERR_RETURN(__kv_set_string(OCGW_KV_HOST, hbuf));
    TUYA_CALL_ERR_RETURN(__kv_set_string(OCGW_KV_TOKEN, tbuf));
    (void)snprintf(pstr, sizeof(pstr), "%u", (unsigned)po);
    TUYA_CALL_ERR_RETURN(__kv_set_string(OCGW_KV_PORT, pstr));

    PR_NOTICE("openclaw_gateway_cfg: saved host=%s port=%u (token len=%u)",
              hbuf, (unsigned)po, (unsigned)strlen(tbuf));
    return OPRT_OK;
}
