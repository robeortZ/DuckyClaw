/**
 * @file tool_todo.h
 * @brief MCP TODO list tools for DuckyClaw
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TOOL_TODO_H__
#define __TOOL_TODO_H__

#include "tuya_cloud_types.h"
#include "ai_mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all TODO MCP tools (todo_add, todo_list, todo_complete, todo_remove)
 *
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET tool_todo_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOOL_TODO_H__ */
