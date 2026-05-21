/**
 * @file openclaw_gateway_cfg.h
 * @brief Runtime OpenClaw ACP gateway host/token/port (tal_kv), with compile-time fallbacks
 * @version 1.0
 * @date 2026-04-16
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __OPENCLAW_GATEWAY_CFG_H__
#define __OPENCLAW_GATEWAY_CFG_H__

#include "tuya_cloud_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPENCLAW_GATEWAY_HOST_MAX
#define OPENCLAW_GATEWAY_HOST_MAX 128
#endif

#ifndef OPENCLAW_GATEWAY_TOKEN_MAX
#define OPENCLAW_GATEWAY_TOKEN_MAX 256
#endif

/**
 * @brief Load effective gateway host (KV override or compile default)
 * @param[out] buf NUL-terminated host or IP
 * @param[in] buf_size Size of buf
 * @return none
 */
void openclaw_gateway_cfg_get_host(char *buf, size_t buf_size);

/**
 * @brief Load effective gateway auth token (KV override or compile default)
 * @param[out] buf NUL-terminated token
 * @param[in] buf_size Size of buf
 * @return none
 */
void openclaw_gateway_cfg_get_token(char *buf, size_t buf_size);

/**
 * @brief Load effective gateway TCP port (KV override or compile default)
 * @return Port in host byte order
 */
uint16_t openclaw_gateway_cfg_get_port(void);

/**
 * @brief Persist gateway settings and replace only fields that are explicitly set
 * @param[in] host If non-NULL and non-empty, save as host (hostname or IPv4)
 * @param[in] token If non-NULL and non-empty, save as token
 * @param[in] port If >0 and <=65535, save as port; otherwise keep current effective port
 * @return OPRT_OK on success
 * @note At least one of host, token, or valid port must be provided; otherwise OPRT_INVALID_PARM
 */
OPERATE_RET openclaw_gateway_cfg_apply_patch(const char *host, const char *token, int32_t port);

#ifdef __cplusplus
}
#endif

#endif /* __OPENCLAW_GATEWAY_CFG_H__ */
