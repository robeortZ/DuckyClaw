/**
 * @file tool_cron.h
 * @brief MCP cron (scheduled task) tools for DuckyClaw
 * @version 0.1
 * @date 2025-03-25
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TOOL_CRON_H__
#define __TOOL_CRON_H__

#include "tuya_cloud_types.h"
#include "ai_mcp_server.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert local calendar date/time (device TZ) to UTC Unix epoch
 * @param[in] year Full year
 * @param[in] month 1-12
 * @param[in] day 1-31
 * @param[in] hour 0-23
 * @param[in] minute 0-59
 * @param[in] second 0-59
 * @return Epoch seconds, or -1 if invalid
 */
int64_t tool_cron_local_datetime_to_epoch(int year, int month, int day, int hour, int minute,
                                          int second);

/**
 * @brief Register all cron MCP tools
 *
 * Registers cron_add, cron_list, and cron_remove tools
 * with the MCP server.
 *
 * @return OPERATE_RET OPRT_OK on success, error code on failure
 */
OPERATE_RET tool_cron_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOOL_CRON_H__ */
