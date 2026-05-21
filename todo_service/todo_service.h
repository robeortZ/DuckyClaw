/**
 * @file todo_service.h
 * @brief TODO list service for DuckyClaw
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * Each task has: id, content, and state (pending / completed).
 * Persisted to todo.json on the filesystem.
 */

#ifndef __TODO_SERVICE_H__
#define __TODO_SERVICE_H__

#include "tuya_cloud_types.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#ifndef CLAW_TODO_MAX_TASKS
#define CLAW_TODO_MAX_TASKS 32
#endif

#ifndef CLAW_TODO_CONTENT_LEN
#define CLAW_TODO_CONTENT_LEN 256
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Single TODO task
 */
typedef struct {
    char  id[9];                    /**< Auto-generated unique ID (8-char hex) */
    char  content[CLAW_TODO_CONTENT_LEN]; /**< Task content */
    bool  completed;                /**< true = 已完成, false = 未完成 */
} todo_task_t;

/***********************************************************
********************function declaration*******************
***********************************************************/

/**
 * @brief Initialize TODO service (load tasks from file)
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET todo_service_init(void);

/**
 * @brief Add a new TODO task (state: 未完成)
 * @param content Task content (will be copied)
 * @param out_id  Optional buffer to receive the new task ID (can be NULL)
 * @param id_size Size of out_id buffer
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET todo_add_task(const char *content, char *out_id, size_t id_size);

/**
 * @brief Insert or update a task from cloud DP key (stable id from cloud_key hash)
 * @param[in] cloud_key Unique id string from cloud (e.g. pipe segment)
 * @param[in] content Task body
 * @param[in] completed Completion flag
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET todo_upsert_from_dp(const char *cloud_key, const char *content, bool completed);

/**
 * @brief List all tasks (caller may filter by completed when displaying)
 * @param tasks Output pointer to internal array (read-only)
 * @param count Output task count
 */
void todo_list_tasks(const todo_task_t **tasks, int *count);

/**
 * @brief Mark a task as completed by ID or by exact content (tries ID first, then content)
 * @param id_or_content Task ID or exact content string
 * @return OPERATE_RET OPRT_OK on success, OPRT_NOT_FOUND if not found
 */
OPERATE_RET todo_complete_task(const char *id_or_content);

/**
 * @brief Remove a task by ID or by exact content (tries ID first, then content)
 * @param id_or_content Task ID or exact content string
 * @return OPERATE_RET OPRT_OK on success, OPRT_NOT_FOUND if not found
 */
OPERATE_RET todo_remove_task(const char *id_or_content);

/**
 * @brief Set completion state by task id (faster path for UI checkboxes)
 * @param id Task id
 * @param completed New state
 * @return OPERATE_RET OPRT_OK on success, OPRT_NOT_FOUND if not found
 */
OPERATE_RET todo_set_completed_by_id(const char *id, bool completed);

#ifdef __cplusplus
}
#endif

#endif /* __TODO_SERVICE_H__ */
