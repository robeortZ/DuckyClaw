/**
 * @file dp_notification_sync.c
 * @brief Sync DP101 notification pipe strings into cron and todo persistence
 * @version 0.1
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * Handles lines: scheduled|cloud_id|YYYY-MM-DD H:mm|content|… and
 * todo|cloud_id||content|true|false (see tuya_app_main.c DP comment).
 */

#include "dp_notification_sync.h"
#include "cron_service.h"
#include "todo_service.h"
#include "tool_cron.h"

#include "tal_api.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */

#define DP_LINE_MAX          512
#define DP_MAX_PIPE_FIELDS    8
#define DP_FIELD_MAX         320

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief Map cloud unique key to 8-char hex id (FNV-1a), same as todo_hash_cloud_key_to_id
 * @param[in] key Non-NULL unique string
 * @param[out] out Buffer >= 9 bytes
 * @param[in] out_sz Size of out
 */
static void __stable_hex_id_from_key(const char *key, char *out, size_t out_sz)
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
 * @brief Split a line by '|' into fixed buffers; does not modify the input line if only read…
 *        (copies segments into buf)
 * @param[in] line Mutable line string (safe to use read-only segments)
 * @param[out] fields Pointers into buf rows
 * @param[out] buf Field storage
 * @param[in] max_fields Max columns
 * @return Number of fields filled
 */
static int __split_pipe(char *line, char *fields[], char buf[][DP_FIELD_MAX], int max_fields)
{
    int n    = 0;
    char *p  = line;

    while (n < max_fields && p) {
        char *sep = strchr(p, '|');
        if (sep) {
            size_t len = (size_t)(sep - p);
            if (len >= DP_FIELD_MAX) {
                len = DP_FIELD_MAX - 1;
            }
            memcpy(buf[n], p, len);
            buf[n][len] = '\0';
            p = sep + 1;
        } else {
            size_t len = strlen(p);
            if (len >= DP_FIELD_MAX) {
                len = DP_FIELD_MAX - 1;
            }
            memcpy(buf[n], p, len);
            buf[n][len] = '\0';
            p = NULL;
        }
        fields[n] = buf[n];
        n++;
        if (!sep) {
            break;
        }
    }
    return n;
}

/**
 * @brief Parse truthy strings for todo completion flag
 * @param[in] s Field text
 * @return true if string means completed
 */
static bool __parse_bool_field(const char *s)
{
    if (!s || s[0] == '\0') {
        return false;
    }
    if (strcmp(s, "1") == 0) {
        return true;
    }
    char low[24];
    size_t i;
    for (i = 0; s[i] && i < sizeof(low) - 1; i++) {
        low[i] = (char)tolower((unsigned char)s[i]);
    }
    low[i] = '\0';
    return (strcmp(low, "true") == 0);
}

/**
 * @brief Parse datetime string YYYY-MM-DD H:M[:S]
 * @param[in] ts Time string
 * @param[out] y Year
 * @param[out] mo Month
 * @param[out] d Day
 * @param[out] h Hour
 * @param[out] mi Minute
 * @param[out] se Second
 */
static void __parse_dt(const char *ts, int *y, int *mo, int *d, int *h, int *mi, int *se)
{
    *se = 0;
    int n = sscanf(ts, "%d-%d-%d %d:%d:%d", y, mo, d, h, mi, se);
    if (n >= 6) {
        return;
    }
    *se = 0;
    (void)sscanf(ts, "%d-%d-%d %d:%d", y, mo, d, h, mi);
}

/**
 * @brief Apply one scheduled|… line to cron_service
 * @param[in] fields Column pointers
 * @param[in] nfields Column count
 */
static void __sync_scheduled_line(char *fields[], int nfields)
{
    if (nfields < 4) {
        PR_WARN("dp_notification: scheduled needs type|id|time|content");
        return;
    }

    const char *cloud_id = fields[1];
    const char *ts       = fields[2];
    const char *content  = fields[3];

    if (!cloud_id || cloud_id[0] == '\0' || !content || content[0] == '\0') {
        PR_WARN("dp_notification: scheduled missing id or content");
        return;
    }

    if (!ts || ts[0] == '\0') {
        PR_WARN("dp_notification: scheduled missing time");
        return;
    }

    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    __parse_dt(ts, &y, &mo, &d, &h, &mi, &se);

    int64_t epoch = tool_cron_local_datetime_to_epoch(y, mo, d, h, mi, se);
    if (epoch < 0) {
        PR_WARN("dp_notification: bad datetime '%s'", ts);
        return;
    }

    int64_t now = (int64_t)tal_time_get_posix();
    if (epoch <= now) {
        PR_WARN("dp_notification: scheduled time in past, skip");
        return;
    }

    char jid[9];
    __stable_hex_id_from_key(cloud_id, jid, sizeof(jid));

    (void)cron_remove_job(jid);

    cron_job_t job;
    memset(&job, 0, sizeof(job));
    snprintf(job.id, sizeof(job.id), "%s", jid);
    strncpy(job.name, content, sizeof(job.name) - 1);
    strncpy(job.message, content, sizeof(job.message) - 1);
    job.kind             = CRON_KIND_AT;
    job.at_epoch         = epoch;
    job.delete_after_run = true;

    OPERATE_RET rt = cron_add_job(&job);
    if (rt != OPRT_OK) {
        PR_WARN("dp_notification: cron_add_job failed %d", rt);
    } else {
        PR_INFO("dp_notification: synced scheduled id=%s", jid);
    }
}

/**
 * @brief Apply one todo|… line to todo_service
 * @param[in] fields Column pointers
 * @param[in] nfields Column count
 */
static void __sync_todo_line(char *fields[], int nfields)
{
    if (nfields < 5) {
        PR_WARN("dp_notification: todo needs type|id|time|content|done");
        return;
    }

    const char *cloud_id = fields[1];
    const char *content  = fields[3];
    const char *done     = fields[4];

    if (!cloud_id || cloud_id[0] == '\0' || !content || content[0] == '\0') {
        PR_WARN("dp_notification: todo missing id or content");
        return;
    }

    bool          completed = __parse_bool_field(done);
    OPERATE_RET rt = todo_upsert_from_dp(cloud_id, content, completed);
    if (rt != OPRT_OK) {
        PR_WARN("dp_notification: todo_upsert_from_dp failed %d", rt);
    } else {
        PR_INFO("dp_notification: synced todo key=%s", cloud_id);
    }
}

/**
 * @brief Process a single trimmed logical line
 * @param[in,out] line Buffer (may be used as split scratch)
 */
static void __sync_one_line(char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (line[0] == '\0') {
        return;
    }

    char  buf[DP_MAX_PIPE_FIELDS][DP_FIELD_MAX];
    char *fields[DP_MAX_PIPE_FIELDS];
    int   nf = __split_pipe(line, fields, buf, DP_MAX_PIPE_FIELDS);
    if (nf < 1 || !fields[0] || fields[0][0] == '\0') {
        return;
    }

    if (strcmp(fields[0], "scheduled") == 0) {
        __sync_scheduled_line(fields, nf);
    } else if (strcmp(fields[0], "todo") == 0) {
        __sync_todo_line(fields, nf);
    }
}

/**
 * @brief Parse pipe-formatted notification lines and update cron/todo stores
 * @param[in] raw Multiline string from DP101 (scheduled|… / todo|… lines)
 */
void ducky_dp_notification_sync(const char *raw)
{
    if (!raw || raw[0] == '\0') {
        return;
    }

    char         line[DP_LINE_MAX];
    const char *start = raw;

    while (start && start[0] != '\0') {
        const char *nl = strchr(start, '\n');
        size_t      len = nl ? (size_t)(nl - start) : strlen(start);
        while (len > 0 && (start[len - 1] == '\r' || start[len - 1] == ' ')) {
            len--;
        }
        if (len >= DP_LINE_MAX) {
            len = DP_LINE_MAX - 1;
        }
        memcpy(line, start, len);
        line[len] = '\0';
        __sync_one_line(line);
        start = nl ? nl + 1 : NULL;
    }
}
