/**
 * @file todo_service.c
 * @brief TODO list service implementation
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "todo_service.h"
#include "tool_files.h"

#include "tal_api.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/***********************************************************
************************macro define************************
***********************************************************/

#define TODO_FILE       CLAW_FS_ROOT_PATH "/todo.json"
#define TODO_FILE_MAX   (16 * 1024)

/***********************************************************
***********************variable define**********************
***********************************************************/

static todo_task_t s_tasks[CLAW_TODO_MAX_TASKS];
static int         s_task_count = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET todo_save_tasks(void);

/**
 * @brief Generate 8-char hex ID for a task
 */
static void todo_generate_id(char *id_buf, size_t id_buf_size)
{
    if (id_buf_size < 9) {
        return;
    }
    uint32_t r = ((uint32_t)tal_system_get_random(0x7fffffffu)) ^
                 (uint32_t)tal_system_get_millisecond();
    snprintf(id_buf, id_buf_size, "%08x", (unsigned int)r);
}

/**
 * @brief Derive stable 8-char hex task id from cloud-side unique key
 * @param[in] key Cloud unique string (e.g. DP pipe field)
 * @param[out] out Id buffer (at least 9 bytes)
 * @param[in] out_sz Size of out
 */
static void todo_hash_cloud_key_to_id(const char *key, char *out, size_t out_sz)
{
    if (!out || out_sz < 9 || !key) {
        if (out && out_sz > 0) {
            out[0] = '\0';
        }
        return;
    }
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    snprintf(out, out_sz, "%08x", (unsigned int)h);
}

/**
 * @brief Load tasks from JSON file
 */
static OPERATE_RET todo_load_tasks(void)
{
    TUYA_FILE f = claw_fopen(TODO_FILE, "r");
    if (!f) {
        PR_DEBUG("No todo file found, starting fresh");
        s_task_count = 0;
        return OPRT_OK;
    }

    int fsize = claw_fgetsize(TODO_FILE);
    if (fsize <= 0 || fsize > TODO_FILE_MAX) {
        claw_fclose(f);
        s_task_count = 0;
        return OPRT_OK;
    }

    char *buf = (char *)claw_malloc((size_t)fsize + 1);
    if (!buf) {
        claw_fclose(f);
        return OPRT_MALLOC_FAILED;
    }

    int n = claw_fread(buf, fsize, f);
    claw_fclose(f);
    if (n < 0) {
        claw_free(buf);
        return OPRT_COM_ERROR;
    }
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    claw_free(buf);
    if (!root) {
        s_task_count = 0;
        return OPRT_OK;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "tasks");
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        s_task_count = 0;
        return OPRT_OK;
    }

    s_task_count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr)
    {
        if (s_task_count >= CLAW_TODO_MAX_TASKS) {
            break;
        }

        const char *id   = cJSON_GetStringValue(cJSON_GetObjectItem(item, "id"));
        const char *cont = cJSON_GetStringValue(cJSON_GetObjectItem(item, "content"));
        if (!id || !cont) {
            continue;
        }

        todo_task_t *t = &s_tasks[s_task_count];
        memset(t, 0, sizeof(*t));
        strncpy(t->id, id, sizeof(t->id) - 1);
        t->id[sizeof(t->id) - 1] = '\0';
        strncpy(t->content, cont, sizeof(t->content) - 1);
        t->content[sizeof(t->content) - 1] = '\0';

        cJSON *completed_j = cJSON_GetObjectItem(item, "completed");
        t->completed = completed_j ? cJSON_IsTrue(completed_j) : false;

        s_task_count++;
    }

    cJSON_Delete(root);
    PR_DEBUG("Loaded %d todo tasks", s_task_count);
    return OPRT_OK;
}

/**
 * @brief Save all tasks to JSON file
 */
static OPERATE_RET todo_save_tasks(void)
{
    cJSON *root  = cJSON_CreateObject();
    cJSON *arr   = cJSON_CreateArray();
    if (!root || !arr) {
        if (root) cJSON_Delete(root);
        if (arr) cJSON_Delete(arr);
        return OPRT_MALLOC_FAILED;
    }

    for (int i = 0; i < s_task_count; i++) {
        todo_task_t *t = &s_tasks[i];
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }
        cJSON_AddStringToObject(item, "id", t->id);
        cJSON_AddStringToObject(item, "content", t->content);
        cJSON_AddBoolToObject(item, "completed", t->completed);
        cJSON_AddItemToArray(arr, item);
    }

    cJSON_AddItemToObject(root, "tasks", arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return OPRT_MALLOC_FAILED;
    }

    TUYA_FILE f = claw_fopen(TODO_FILE, "w");
    if (!f) {
        cJSON_free(json_str);
        PR_ERR("Failed to open %s for writing", TODO_FILE);
        return OPRT_COM_ERROR;
    }

    size_t len = strlen(json_str);
    int wn = claw_fwrite(json_str, (int)len, f);
    claw_fclose(f);
    cJSON_free(json_str);

    if (wn < 0 || (size_t)wn != len) {
        PR_ERR("Todo save incomplete");
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

OPERATE_RET todo_service_init(void)
{
    return todo_load_tasks();
}

OPERATE_RET todo_upsert_from_dp(const char *cloud_key, const char *content, bool completed)
{
    if (!cloud_key || cloud_key[0] == '\0' || !content || content[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    char id[9];
    todo_hash_cloud_key_to_id(cloud_key, id, sizeof(id));

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].id, id) == 0) {
            strncpy(s_tasks[i].content, content, sizeof(s_tasks[i].content) - 1);
            s_tasks[i].content[sizeof(s_tasks[i].content) - 1] = '\0';
            s_tasks[i].completed = completed;
            return todo_save_tasks();
        }
    }

    if (s_task_count >= CLAW_TODO_MAX_TASKS) {
        return OPRT_RESOURCE_NOT_READY;
    }

    todo_task_t *t = &s_tasks[s_task_count];
    memset(t, 0, sizeof(*t));
    snprintf(t->id, sizeof(t->id), "%s", id);
    strncpy(t->content, content, sizeof(t->content) - 1);
    t->content[sizeof(t->content) - 1] = '\0';
    t->completed = completed;
    s_task_count++;
    return todo_save_tasks();
}

OPERATE_RET todo_add_task(const char *content, char *out_id, size_t id_size)
{
    if (!content || content[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    if (s_task_count >= CLAW_TODO_MAX_TASKS) {
        return OPRT_RESOURCE_NOT_READY;
    }

    todo_task_t *t = &s_tasks[s_task_count];
    memset(t, 0, sizeof(*t));
    todo_generate_id(t->id, sizeof(t->id));
    strncpy(t->content, content, sizeof(t->content) - 1);
    t->content[sizeof(t->content) - 1] = '\0';
    t->completed = false;

    s_task_count++;
    (void)todo_save_tasks();

    if (out_id && id_size > 0) {
        snprintf(out_id, id_size, "%s", t->id);
    }
    PR_INFO("Todo added: %s (%s)", t->content, t->id);
    return OPRT_OK;
}

void todo_list_tasks(const todo_task_t **tasks, int *count)
{
    if (tasks) {
        *tasks = s_tasks;
    }
    if (count) {
        *count = s_task_count;
    }
}

OPERATE_RET todo_complete_task(const char *id_or_content)
{
    if (!id_or_content || id_or_content[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].id, id_or_content) == 0) {
            s_tasks[i].completed = true;
            (void)todo_save_tasks();
            PR_INFO("Todo completed by id: %s", s_tasks[i].id);
            return OPRT_OK;
        }
    }

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].content, id_or_content) == 0) {
            s_tasks[i].completed = true;
            (void)todo_save_tasks();
            PR_INFO("Todo completed by content: %s", s_tasks[i].id);
            return OPRT_OK;
        }
    }

    return OPRT_NOT_FOUND;
}

OPERATE_RET todo_set_completed_by_id(const char *id, bool completed)
{
    if (!id || id[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].id, id) == 0) {
            s_tasks[i].completed = completed;
            (void)todo_save_tasks();
            return OPRT_OK;
        }
    }

    return OPRT_NOT_FOUND;
}

OPERATE_RET todo_remove_task(const char *id_or_content)
{
    if (!id_or_content || id_or_content[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].id, id_or_content) == 0) {
            for (int j = i; j < s_task_count - 1; j++) {
                s_tasks[j] = s_tasks[j + 1];
            }
            s_task_count--;
            (void)todo_save_tasks();
            PR_INFO("Todo removed by id: %s", id_or_content);
            return OPRT_OK;
        }
    }

    for (int i = 0; i < s_task_count; i++) {
        if (strcmp(s_tasks[i].content, id_or_content) == 0) {
            for (int j = i; j < s_task_count - 1; j++) {
                s_tasks[j] = s_tasks[j + 1];
            }
            s_task_count--;
            (void)todo_save_tasks();
            PR_INFO("Todo removed by content");
            return OPRT_OK;
        }
    }

    return OPRT_NOT_FOUND;
}
