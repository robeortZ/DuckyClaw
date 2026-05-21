/**
 * @file tool_openclaw_gateway.h
 * @brief MCP tools: openclaw_gateway_get / openclaw_gateway_set
 * @version 1.0
 * @date 2026-04-16
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TOOL_OPENCLAW_GATEWAY_H__
#define __TOOL_OPENCLAW_GATEWAY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register OpenClaw gateway configuration MCP tools
 * @return OPRT_OK on success
 */
OPERATE_RET tool_openclaw_gateway_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOOL_OPENCLAW_GATEWAY_H__ */
