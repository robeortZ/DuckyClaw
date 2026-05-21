/**
 * @file tool_todo.c
 * @brief MCP TODO list tools for DuckyClaw
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * Each task has: content (任务内容), state 未完成/已完成.
 * Tools: todo_add, todo_list, todo_complete, todo_remove.
 */

#include "tool_todo.h"
#include "todo_service.h"

#include "tal_api.h"
#include "tool_files.h"

#include <string.h>
#include <stdlib.h>

/***********************************************************
***********************function define**********************
***********************************************************/

static const char *__get_str_prop(const MCP_PROPERTY_LIST_T *properties, const char *name)
{
    const MCP_PROPERTY_T *prop = ai_mcp_property_list_find(properties, name);
    if (prop && prop->type == MCP_PROPERTY_TYPE_STRING) {
        return prop->default_val.str_val;
    }
    return NULL;
}

static OPERATE_RET __tool_todo_add(const MCP_PROPERTY_LIST_T *properties,
                                   MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *content = __get_str_prop(properties, "content");
    if (!content || content[0] == '\0') {
        ai_mcp_return_value_set_str(ret_val, "Error: missing or empty 'content'");
        return OPRT_INVALID_PARM;
    }

    char new_id[16] = {0};
    OPERATE_RET rt = todo_add_task(content, new_id, sizeof(new_id));

    char result[320];
    if (rt == OPRT_OK) {
        snprintf(result, sizeof(result), "OK: 已添加待办 \"%s\" (id=%s)，状态：未完成。", content, new_id);
    } else if (rt == OPRT_RESOURCE_NOT_READY) {
        snprintf(result, sizeof(result), "Error: 待办数量已达上限");
    } else {
        snprintf(result, sizeof(result), "Error: 添加失败 (rt=%d)", rt);
    }

    ai_mcp_return_value_set_str(ret_val, result);
    return (rt == OPRT_OK) ? OPRT_OK : rt;
}

static OPERATE_RET __tool_todo_list(const MCP_PROPERTY_LIST_T *properties,
                                   MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *filter = __get_str_prop(properties, "filter");
    if (!filter || filter[0] == '\0') {
        filter = "all";
    }
    if (strcmp(filter, "active") != 0 && strcmp(filter, "completed") != 0 && strcmp(filter, "all") != 0) {
        filter = "all";
    }

    const todo_task_t *tasks = NULL;
    int count = 0;
    todo_list_tasks(&tasks, &count);

    if (count == 0) {
        ai_mcp_return_value_set_str(ret_val, "暂无待办任务。");
        return OPRT_OK;
    }

    size_t buf_size = 4096;
    char *buf = (char *)claw_malloc(buf_size);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }

    size_t off = 0;
    off += (size_t)snprintf(buf + off, buf_size - off, "TODO 列表 (共 %d 项", count);
    if (strcmp(filter, "active") == 0) {
        off += (size_t)snprintf(buf + off, buf_size - off, "，仅未完成");
    } else if (strcmp(filter, "completed") == 0) {
        off += (size_t)snprintf(buf + off, buf_size - off, "，仅已完成");
    }
    off += (size_t)snprintf(buf + off, buf_size - off, "):\n");

    int shown = 0;
    for (int i = 0; i < count && off < buf_size - 256; i++) {
        const todo_task_t *t = &tasks[i];
        if (strcmp(filter, "active") == 0 && t->completed) {
            continue;
        }
        if (strcmp(filter, "completed") == 0 && !t->completed) {
            continue;
        }
        shown++;
        off += (size_t)snprintf(buf + off, buf_size - off,
                               "  %d. [%s] %s - %s\n",
                               shown, t->id, t->content,
                               t->completed ? "已完成" : "未完成");
    }

    ai_mcp_return_value_set_str(ret_val, buf);
    claw_free(buf);
    return OPRT_OK;
}

static OPERATE_RET __tool_todo_complete(const MCP_PROPERTY_LIST_T *properties,
                                       MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *task_id  = __get_str_prop(properties, "task_id");
    const char *content = __get_str_prop(properties, "content");
    const char *id_or_content = NULL;

    if (task_id && task_id[0] != '\0') {
        id_or_content = task_id;
    } else if (content && content[0] != '\0') {
        id_or_content = content;
    }
    if (!id_or_content) {
        ai_mcp_return_value_set_str(ret_val, "Error: 请提供 task_id 或 content（来自 todo_list）");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = todo_complete_task(id_or_content);

    char result[192];
    if (rt == OPRT_OK) {
        snprintf(result, sizeof(result), "OK: 已将任务标记为已完成 (id_or_content=%s)。", id_or_content);
    } else if (rt == OPRT_NOT_FOUND) {
        snprintf(result, sizeof(result), "Error: 未找到该任务，请用 todo_list 查看当前列表。");
    } else {
        snprintf(result, sizeof(result), "Error: 操作失败 (rt=%d)", rt);
    }

    ai_mcp_return_value_set_str(ret_val, result);
    return (rt == OPRT_OK) ? OPRT_OK : rt;
}

static OPERATE_RET __tool_todo_remove(const MCP_PROPERTY_LIST_T *properties,
                                     MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *task_id  = __get_str_prop(properties, "task_id");
    const char *content = __get_str_prop(properties, "content");
    const char *id_or_content = NULL;

    if (task_id && task_id[0] != '\0') {
        id_or_content = task_id;
    } else if (content && content[0] != '\0') {
        id_or_content = content;
    }
    if (!id_or_content) {
        ai_mcp_return_value_set_str(ret_val, "Error: 请提供 task_id 或 content（来自 todo_list）");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = todo_remove_task(id_or_content);

    char result[192];
    if (rt == OPRT_OK) {
        snprintf(result, sizeof(result), "OK: 已删除任务 (id_or_content=%s)。", id_or_content);
    } else if (rt == OPRT_NOT_FOUND) {
        snprintf(result, sizeof(result), "Error: 未找到该任务，请用 todo_list 查看当前列表。");
    } else {
        snprintf(result, sizeof(result), "Error: 删除失败 (rt=%d)", rt);
    }

    ai_mcp_return_value_set_str(ret_val, result);
    return (rt == OPRT_OK) ? OPRT_OK : rt;
}

OPERATE_RET tool_todo_register(void)
{
    OPERATE_RET rt = OPRT_OK;
    /* todo_add */
    TUYA_CALL_ERR_RETURN(AI_MCP_TOOL_ADD(
        "todo_add",
        "Add a new TODO task (state: 未完成).\n"
        "Parameters:\n"
        "- content (string): Task content.\n"
        "Response: Task id and confirmation.",
        __tool_todo_add,
        NULL,
        MCP_PROP_STR("content", "Task content (任务内容)")
    ));

    /* todo_list */
    TUYA_CALL_ERR_RETURN(AI_MCP_TOOL_ADD(
        "todo_list",
        "List TODO tasks. Optional filter: all (default), active (未完成), completed (已完成).\n"
        "Parameters:\n"
        "- filter (string, optional): 'all' | 'active' | 'completed'.\n"
        "Response: Numbered list with id, content, and state.",
        __tool_todo_list,
        NULL,
        MCP_PROP_STR_DEF("filter", "Filter: all / active / completed", "all")
    ));

    /* todo_complete: by task_id or content (optional params) */
    TUYA_CALL_ERR_RETURN(AI_MCP_TOOL_ADD(
        "todo_complete",
        "Mark a TODO task as 已完成. Provide either task_id or content (from todo_list).\n"
        "Parameters:\n"
        "- task_id (string, optional): Task ID from todo_list.\n"
        "- content (string, optional): Exact task content. At least one required.\n"
        "Response: Confirmation or error.",
        __tool_todo_complete,
        NULL,
        MCP_PROP_STR_DEF("task_id", "Task ID from todo_list", ""),
        MCP_PROP_STR_DEF("content", "Exact task content", "")
    ));

    /* todo_remove: by task_id or content */
    TUYA_CALL_ERR_RETURN(AI_MCP_TOOL_ADD(
        "todo_remove",
        "Remove a TODO task. Provide either task_id or content (from todo_list).\n"
        "Parameters:\n"
        "- task_id (string, optional): Task ID from todo_list.\n"
        "- content (string, optional): Exact task content. At least one required.\n"
        "Response: Confirmation or error.",
        __tool_todo_remove,
        NULL,
        MCP_PROP_STR_DEF("task_id", "Task ID from todo_list", ""),
        MCP_PROP_STR_DEF("content", "Exact task content", "")
    ));

    PR_DEBUG("TODO MCP tools registered");
    return OPRT_OK;
}
