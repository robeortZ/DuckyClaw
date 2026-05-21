/**
 * @file ai_audio_player.c
 * @brief This file contains the implementation of the audio player module, which is responsible for playing audio
 * streams.
 *
 * @version 0.1
 * @date 2025-03-25
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#define MINIMP3_IMPLEMENTATION

#include "tkl_system.h"
#include "tkl_memory.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"
#include "svc_ai_player.h"

#include "tdl_audio_manage.h"

#include "media_src.h"
#include "ai_user_event.h"
#include "ai_agent.h"
#include "ai_audio_player.h"
#include "tal_workq_service.h"
#include "tal_system.h"

#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_MUSIC_META_CAP 32
/** Poll interval while waiting for foreground (TTS) player to finish decoding */
#define PENDING_MUSIC_FG_POLL_MS  200U
/** Max polls (~50s) then drop deferred music */
#define PENDING_MUSIC_FG_MAX_POLL 250U
/** Brief pause after FG idle before starting BG HTTPS (reduce overlap with stack/teardown) */
#define PENDING_MUSIC_POST_FG_DELAY_MS 250U

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char *song_name;
    char *artist;
    char *img_url;
} __music_meta_slot_t;

/***********************************************************
********************function declaration********************
***********************************************************/
static char *__dup_str_opt(const char *s);
static void __music_meta_clear(void);
static void __music_struct_free(AI_AUDIO_MUSIC_T *music);
static AI_AUDIO_MUSIC_T *__music_struct_dup(const AI_AUDIO_MUSIC_T *src);
static OPERATE_RET __ai_audio_play_music_impl(AI_AUDIO_MUSIC_T *music);
static void __pending_music_play_after_tts(void);
static void __pending_music_deferred_kick(void *data);

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool __s_music_continuous = false;
static bool __s_music_replay = false;
static bool __s_tts_play_flag = false;
static AI_PLAYER_HANDLE __s_tone_player = NULL;
static AI_PLAYLIST_HANDLE __s_tone_playlist = NULL;
static AI_PLAYER_HANDLE __s_music_player = NULL;
static AI_PLAYLIST_HANDLE __s_music_playlist = NULL;
static AI_PLAYER_ALERT_CUSTOM_CB __s_alert_custom_cb = NULL;
static __music_meta_slot_t s_music_meta[AI_MUSIC_META_CAP];
static uint32_t s_music_meta_cnt;
/** Deferred playlist when skill sets has_tts (avoid concurrent HTTPS with TTS) */
static AI_AUDIO_MUSIC_T *s_pending_music = NULL;
static uint32_t          s_pending_fg_wait_attempt;
static DELAYED_WORK_HANDLE s_pending_music_delayed = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Free a duplicated AI_AUDIO_MUSIC_T and nested URL strings
 * @param[in] music Music struct or NULL
 * @return none
 */
static void __music_struct_free(AI_AUDIO_MUSIC_T *music)
{
    int i;

    if (music == NULL) {
        return;
    }
    if (music->src_array != NULL) {
        for (i = 0; i < music->src_cnt; i++) {
            AI_MUSIC_SRC_T *s = &music->src_array[i];

            if (s->url != NULL) {
                tal_free(s->url);
            }
            if (s->artist != NULL) {
                tal_free(s->artist);
            }
            if (s->song_name != NULL) {
                tal_free(s->song_name);
            }
            if (s->audio_id != NULL) {
                tal_free(s->audio_id);
            }
            if (s->img_url != NULL) {
                tal_free(s->img_url);
            }
        }
        tal_free(music->src_array);
    }
    tal_free(music);
}

/**
 * @brief Deep-copy music struct for deferred playback after TTS
 * @param[in] src Source structure from skill parser
 * @return Copy or NULL on OOM
 */
static AI_AUDIO_MUSIC_T *__music_struct_dup(const AI_AUDIO_MUSIC_T *src)
{
    AI_AUDIO_MUSIC_T *dst;
    int                 i;

    if (src == NULL) {
        return NULL;
    }
    dst = (AI_AUDIO_MUSIC_T *)tal_malloc(sizeof(AI_AUDIO_MUSIC_T));
    if (dst == NULL) {
        return NULL;
    }
    memcpy(dst, src, sizeof(AI_AUDIO_MUSIC_T));
    dst->src_array = NULL;
    if (src->src_cnt <= 0) {
        return dst;
    }
    dst->src_array = (AI_MUSIC_SRC_T *)tal_malloc(sizeof(AI_MUSIC_SRC_T) * (size_t)src->src_cnt);
    if (dst->src_array == NULL) {
        tal_free(dst);
        return NULL;
    }
    memset(dst->src_array, 0, sizeof(AI_MUSIC_SRC_T) * (size_t)src->src_cnt);
    for (i = 0; i < src->src_cnt; i++) {
        AI_MUSIC_SRC_T *ds = &dst->src_array[i];
        const AI_MUSIC_SRC_T *ss = &src->src_array[i];

        memcpy(ds, ss, sizeof(AI_MUSIC_SRC_T));
        ds->url = __dup_str_opt(ss->url);
        ds->artist = __dup_str_opt(ss->artist);
        ds->song_name = __dup_str_opt(ss->song_name);
        ds->audio_id = __dup_str_opt(ss->audio_id);
        ds->img_url = __dup_str_opt(ss->img_url);
    }
    return dst;
}

static char *__dup_str_opt(const char *s)
{
    size_t n;
    char *p;

    if (s == NULL) {
        return NULL;
    }
    n = strlen(s) + 1;
    p = (char *)tal_malloc(n);
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

static void __music_meta_clear(void)
{
    uint32_t i;

    for (i = 0; i < s_music_meta_cnt; i++) {
        if (s_music_meta[i].song_name) {
            tal_free(s_music_meta[i].song_name);
        }
        if (s_music_meta[i].artist) {
            tal_free(s_music_meta[i].artist);
        }
        if (s_music_meta[i].img_url) {
            tal_free(s_music_meta[i].img_url);
        }
        memset(&s_music_meta[i], 0, sizeof(s_music_meta[i]));
    }
    s_music_meta_cnt = 0;
}

#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)

OPERATE_RET __player_local_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *audio_data = NULL;
    uint32_t audio_size = 0;

    switch(type) {
    case AI_AUDIO_ALERT_POWER_ON:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_POWER_ON;
        audio_size = sizeof(LOCAL_ALERT_SRC_POWER_ON);
    break;
    case AI_AUDIO_ALERT_NOT_ACTIVE:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NOT_ACTIVE;
        audio_size = sizeof(LOCAL_ALERT_SRC_NOT_ACTIVE);
    break;
    case AI_AUDIO_ALERT_NETWORK_CFG:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_CFG;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_CFG);
    break;
    case AI_AUDIO_ALERT_NETWORK_FAIL:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_FAILED;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_FAILED);
    break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_DISCONNECT;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_DISCONNECT);
    break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_CONNECTED;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_CONNECTED);
    break;
    case AI_AUDIO_ALERT_BATTERY_LOW:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_LOW_BATTERY;
        audio_size = sizeof(LOCAL_ALERT_SRC_LOW_BATTERY);
    break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_PLEASE_AGAIN;
        audio_size = sizeof(LOCAL_ALERT_SRC_PLEASE_AGAIN);
    break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_LONG_KEY_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_LONG_KEY_TALK);
    break;
    case AI_AUDIO_ALERT_KEY_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_KEY_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_KEY_TALK);
    break;
    case AI_AUDIO_ALERT_WAKEUP_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_WAKEUP_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_WAKEUP_TALK);
    break;
    case AI_AUDIO_ALERT_RANDOM_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_FREE_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_FREE_TALK);
    break;
    case AI_AUDIO_ALERT_WAKEUP: 
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_WAKEUP;
        audio_size = sizeof(LOCAL_ALERT_SRC_WAKEUP);
    break;
    default:
        PR_NOTICE("audio player -> local alert type: %d not support", type);
        break;
    }

    if(audio_data && audio_size) {
        TUYA_CALL_ERR_LOG(ai_audio_play_data(AI_AUDIO_CODEC_MP3, audio_data, audio_size));
    }

    return rt;
}

#endif

#if defined(AI_AGENT_ENABLE_CLOUD_ALERT) && (AI_AGENT_ENABLE_CLOUD_ALERT == 1)
OPERATE_RET __player_cloud_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    rt = ai_agent_cloud_alert(type);
    if(rt != OPRT_OK) {
        TUYA_CALL_ERR_LOG(ai_audio_play_data(AI_AUDIO_CODEC_MP3, \
                        (uint8_t*)media_src_dingdong, \
                        sizeof(media_src_dingdong)));
    }

    return rt;
}

#endif


/**
@brief Player event callback function
@param data Pointer to player event data
@return OPERATE_RET Operation result
*/
static OPERATE_RET __player_event(void *data)
{
    TUYA_CHECK_NULL_RETURN(data, OPRT_OK);    

    AI_PLAYER_EVT_T *event = (AI_PLAYER_EVT_T*)data;
    PR_DEBUG("audio player -> player %s event: %d", (event->handle == __s_tone_player) ? "tts" : "music", event->state);

    OPERATE_RET rt = OPRT_OK;
    int player_vol = 0;
    tuya_ai_player_get_volume(NULL, &player_vol);
    /* Player finished or failed */
    if (event->state == AI_PLAYER_STOPPED) {
        PR_DEBUG("audio player -> stop event");
        if (!ai_audio_player_is_playing()) {
            ai_user_event_notify(AI_USER_EVT_PLAY_END, NULL);
        }else {
			PR_DEBUG("audio player -> playing stop event, music vol change to %d", player_vol);
			tuya_ai_player_set_volume(__s_music_player, player_vol);	
		}

        /* If TTS play stop, reset play flag */
        if(event->handle == __s_tone_player && __s_tts_play_flag) {
            __s_tts_play_flag = FALSE;
        }
    }
    else if (event->state == AI_PLAYER_PLAYING) {
        PR_DEBUG("audio player -> playing start event");
        if (event->handle == __s_tone_player && AI_PLAYER_PLAYING == tuya_ai_player_get_state(__s_music_player)) {
            PR_DEBUG("audio player -> playing start event, music vol change to %d", player_vol/2);
            tuya_ai_player_set_volume(__s_music_player, player_vol/2);
        }
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_PLAY, NULL);
    } else if (event->state == AI_PLAYER_PAUSED) {
        PR_DEBUG("audio player -> pause event");
        /* Do not notify AI_USER_EVT_PLAY_CTL_PAUSE: ai_chat_main stops BG player and
         * clears the playlist on that event. UI/voice soft pause must preserve the queue. */
    }
    
    return rt;
}

/**
@brief Initialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    /* Player init */
    AI_PLAYER_CFG_T cfg = {.sample = 16000, .datebits = 16, .channel = 1};
    TUYA_CALL_ERR_GOTO(tuya_ai_player_service_init(&cfg), __error);

    /* TTS player */
    TUYA_CALL_ERR_GOTO(tuya_ai_player_create(AI_PLAYER_MODE_FOREGROUND, &__s_tone_player), __error);

    AI_PLAYLIST_CFG_T ton_cfg = {.auto_play = true,.capacity = 2};
    TUYA_CALL_ERR_GOTO(tuya_ai_playlist_create(__s_tone_player, &ton_cfg, &__s_tone_playlist), __error);

    /* Music player */
    TUYA_CALL_ERR_GOTO(tuya_ai_player_create(AI_PLAYER_MODE_BACKGROUND, &__s_music_player), __error);

    AI_PLAYLIST_CFG_T misc_cfg = {.auto_play = true,.capacity = 32};
    TUYA_CALL_ERR_GOTO(tuya_ai_playlist_create(__s_music_player, &misc_cfg, &__s_music_playlist), __error);

    /* Player state */
    TUYA_CALL_ERR_GOTO(tal_event_subscribe(EVENT_AI_PLAYER_STATE, "ai_player", __player_event, SUBSCRIBE_TYPE_NORMAL), __error);

    TUYA_CALL_ERR_GOTO(tal_workq_init_delayed(WORKQ_SYSTEM, __pending_music_deferred_kick, NULL, &s_pending_music_delayed), __error);

    return rt;

__error:
    ai_audio_player_deinit();

    return rt;
}

/**
@brief Deinitialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    __music_meta_clear();
    if (s_pending_music_delayed != NULL) {
        tal_workq_cancel_delayed(s_pending_music_delayed);
        s_pending_music_delayed = NULL;
    }
    __music_struct_free(s_pending_music);
    s_pending_music = NULL;
    s_pending_fg_wait_attempt = 0;

    if (__s_tone_player) {
        TUYA_CALL_ERR_LOG(tuya_ai_player_destroy(__s_tone_player));
        __s_tone_player = NULL;
    }
    
    if (__s_tone_playlist) {
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_destroy(__s_tone_playlist));
        __s_tone_playlist = NULL;
    }

    if (__s_music_player) {
        TUYA_CALL_ERR_LOG(tuya_ai_player_destroy(__s_music_player));
        __s_music_player = NULL;
    }
    
    if (__s_music_playlist) {
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_destroy(__s_music_playlist));
        __s_music_playlist = NULL;
    }

    /* Player deinit */
    TUYA_CALL_ERR_RETURN(tuya_ai_player_service_deinit());
    
    return rt;
}

/**
@brief Set music continuous play flag
@param is_music_continuous Continuous play flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_resume(bool is_music_continuous)
{
    __s_music_continuous = is_music_continuous;
    return OPRT_OK;
}

/**
@brief Set music replay flag
@param is_music_replay Replay flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_replay(bool is_music_replay)
{
    __s_music_replay = is_music_replay;
    return OPRT_OK;
}

/**
@brief Check if audio player is currently playing
@return uint8_t Returns TRUE if playing, FALSE otherwise
*/
uint8_t ai_audio_player_is_playing(void)
{
    if (tuya_ai_player_get_state(__s_tone_player) == AI_PLAYER_PLAYING ||
        tuya_ai_player_get_state(__s_music_player) == AI_PLAYER_PLAYING) {
            return TRUE;
    } 

    return FALSE;
}

/**
 * @brief Fill playlist and metadata (immediate music start)
 * @param[in] music Parsed music from skill
 * @return OPERATE_RET
 */
static OPERATE_RET __ai_audio_play_music_impl(AI_AUDIO_MUSIC_T *music)
{
    OPERATE_RET rt = OPRT_OK;
    int         n;
    int         i;

    if (music->src_cnt <= 0) {
        PR_ERR("music src cnt is 0");
        return rt;
    }

    __music_meta_clear();
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));

    n = music->src_cnt;
    if (n > AI_MUSIC_META_CAP) {
        n = AI_MUSIC_META_CAP;
    }
    for (i = 0; i < n; i++) {
        s_music_meta[i].song_name = __dup_str_opt(music->src_array[i].song_name);
        s_music_meta[i].artist = __dup_str_opt(music->src_array[i].artist);
        s_music_meta[i].img_url = __dup_str_opt(music->src_array[i].img_url);
    }
    s_music_meta_cnt = (uint32_t)n;

    for (i = 0; i < music->src_cnt; i++) {
        PR_DEBUG("audio player -> player music url %s", music->src_array[i].url);
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_music_playlist, AI_PLAYER_SRC_URL,\
                                               music->src_array[i].url, music->src_array[i].format));
    }

    return rt;
}

/**
 * @brief Start deferred music after TTS pipeline is idle
 * @return none
 */
static void __pending_music_play_after_tts(void)
{
    AI_AUDIO_MUSIC_T *m = s_pending_music;

    if (m == NULL) {
        return;
    }
    s_pending_music = NULL;
    s_pending_fg_wait_attempt = 0;
    tal_system_sleep(PENDING_MUSIC_POST_FG_DELAY_MS);
    if (__ai_audio_play_music_impl(m) != OPRT_OK) {
        PR_ERR("audio player -> deferred music play failed");
    }
    __music_struct_free(m);
}

/**
 * @brief Workqueue: wait until foreground player not PLAYING, then start BG music
 * @param[in] data Unused
 * @return none
 */
static void __pending_music_deferred_kick(void *data)
{
    OPERATE_RET rt;

    (void)data;
    if (s_pending_music == NULL) {
        return;
    }
    if (tuya_ai_player_get_state(__s_tone_player) == AI_PLAYER_PLAYING) {
        s_pending_fg_wait_attempt++;
        if (s_pending_fg_wait_attempt >= PENDING_MUSIC_FG_MAX_POLL) {
            PR_WARN("audio player -> drop pending music (FG still playing)");
            __music_struct_free(s_pending_music);
            s_pending_music = NULL;
            s_pending_fg_wait_attempt = 0;
            return;
        }
        rt = tal_workq_start_delayed(s_pending_music_delayed, PENDING_MUSIC_FG_POLL_MS, LOOP_ONCE);
        if (rt != OPRT_OK) {
            PR_ERR("audio player -> pending music reschedule failed %d", rt);
        }
        return;
    }
    __pending_music_play_after_tts();
}

/**
@brief Play music from playlist
@param music Pointer to music structure containing playlist
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_music(AI_AUDIO_MUSIC_T *music)
{
    TUYA_CHECK_NULL_RETURN(music, OPRT_INVALID_PARM);

    if (music->src_cnt <= 0) {
        PR_ERR("music src cnt is 0");
        return OPRT_OK;
    }

    if (music->has_tts) {
        __music_struct_free(s_pending_music);
        s_pending_music = __music_struct_dup(music);
        if (s_pending_music == NULL) {
            PR_ERR("audio player -> defer music dup failed");
            return OPRT_MALLOC_FAILED;
        }
        s_pending_fg_wait_attempt = 0;
        PR_NOTICE("audio player -> defer music until TTS/FG idle (has_tts)");
        return OPRT_OK;
    }

    __music_struct_free(s_pending_music);
    s_pending_music = NULL;
    return __ai_audio_play_music_impl(music);
}

/**
@brief Get cached UI metadata for the current playlist item
@param meta Output metadata pointers
@return OPRT_OK or error
*/
OPERATE_RET ai_audio_player_get_current_music_meta(AI_AUDIO_MUSIC_UI_META_T *meta)
{
    AI_PLAYLIST_INFO_T info;
    uint32_t idx;

    TUYA_CHECK_NULL_RETURN(meta, OPRT_INVALID_PARM);
    memset(meta, 0, sizeof(*meta));
    memset(&info, 0, sizeof(info));

    if (tuya_ai_playlist_get_info(__s_music_playlist, &info) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (info.count == 0 || s_music_meta_cnt == 0) {
        return OPRT_OK;
    }
    idx = info.index;
    if (idx >= s_music_meta_cnt) {
        idx = s_music_meta_cnt - 1;
    }
    meta->song_name = s_music_meta[idx].song_name;
    meta->artist = s_music_meta[idx].artist;
    meta->img_url = s_music_meta[idx].img_url;
    return OPRT_OK;
}

/**
@brief Background music player state for UI
@return Idle / playing / paused
*/
AI_AUDIO_MUSIC_UI_STATE_E ai_audio_player_music_ui_state(void)
{
    AI_PLAYER_STATE_T s = tuya_ai_player_get_state(__s_music_player);

    if (s == AI_PLAYER_PLAYING) {
        return AI_AUDIO_MUSIC_UI_PLAYING;
    }
    if (s == AI_PLAYER_PAUSED) {
        return AI_AUDIO_MUSIC_UI_PAUSED;
    }
    return AI_AUDIO_MUSIC_UI_IDLE;
}

/**
@brief Pause or resume background music without destroying the playlist
@param pause TRUE to pause
@return OPERATE_RET
*/
OPERATE_RET ai_audio_player_music_pause_set(bool pause)
{
    if (pause) {
        return tuya_ai_player_pause(__s_music_player);
    }
    return tuya_ai_player_resume(__s_music_player);
}

/**
@brief Skip to previous item in the background playlist
@return OPERATE_RET
*/
OPERATE_RET ai_audio_player_music_skip_prev(void)
{
    return tuya_ai_playlist_prev(__s_music_playlist);
}

/**
@brief Skip to next item in the background playlist
@return OPERATE_RET
*/
OPERATE_RET ai_audio_player_music_skip_next(void)
{
    return tuya_ai_playlist_next(__s_music_playlist);
}

/**
@brief Play TTS from URL
@param playtts Pointer to TTS play structure
@param is_loop Loop flag (unused)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_url(AI_AUDIO_PLAY_TTS_T *playtts, bool is_loop)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(playtts, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(playtts->tts.url, OPRT_INVALID_PARM);

    PR_DEBUG("audio player -> player tts url %s", playtts->tts.url);
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_tone_playlist, AI_PLAYER_SRC_URL, \
                                           playtts->tts.url, playtts->tts.format));

    return rt;
}

/**
@brief Play audio data from memory
@param format Audio codec format
@param data Pointer to audio data
@param len Audio data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_data(AI_AUDIO_CODEC_E format, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tuya_ai_playlist_stop(__s_tone_playlist));

    if (data && len > 0) {
        PR_NOTICE("audio player -> player tts mem data len %d", len);
        TUYA_CALL_ERR_LOG(tuya_ai_player_start(__s_tone_player, AI_PLAYER_SRC_MEM, NULL, format));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, (uint8_t *)data, len));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
    } 

    return rt;
}

/**
@brief Play TTS stream data
@param state TTS stream state (START, DATA, STOP, ABORT)
@param codec Audio codec format
@param data Pointer to TTS data
@param len TTS data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STATE_E state, AI_AUDIO_CODEC_E codec, char *data,  int len)
{
    OPERATE_RET rt = OPRT_OK;
    
    switch(state) {
    case AI_AUDIO_PLAYER_TTS_START:
        PR_DEBUG("audio player -> tts stream start");
        __s_tts_play_flag = TRUE;
        ai_user_event_notify(AI_USER_EVT_TTS_PRE, NULL);
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_start(__s_tone_player, AI_PLAYER_SRC_MEM, NULL, codec));
        ai_user_event_notify(AI_USER_EVT_TTS_START, NULL);
        break;
    case AI_AUDIO_PLAYER_TTS_DATA:
        // PR_DEBUG("audio player -> tts stream data %d", len);        
        if (data && len > 0 && __s_tts_play_flag) {
            TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, (uint8_t *)data, len));
        }
        ai_user_event_notify(AI_USER_EVT_TTS_DATA, NULL);
        break;
    case AI_AUDIO_PLAYER_TTS_STOP:
        PR_DEBUG("audio player -> tts stream stop");
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
        ai_user_event_notify(AI_USER_EVT_TTS_STOP, NULL);
        if (s_pending_music != NULL) {
            s_pending_fg_wait_attempt = 0;
            rt = tal_workq_schedule(WORKQ_SYSTEM, __pending_music_deferred_kick, NULL);
            if (rt != OPRT_OK) {
                PR_ERR("audio player -> pending music kick schedule failed %d", rt);
            }
        }
        break;
    case AI_AUDIO_PLAYER_TTS_ABORT:
        PR_DEBUG("audio player -> tts stream abort");
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
        ai_user_event_notify(AI_USER_EVT_TTS_ABORT, NULL);
        __music_struct_free(s_pending_music);
        s_pending_music = NULL;
        s_pending_fg_wait_attempt = 0;
        break;
    default:
        break;
    }

    return rt;
}

/**
@brief Play local audio file
@param url Audio file URL
@param song_name Song name (unused)
@param artist Artist name (unused)
@param format Audio format
@param size File size (unused)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_local(char *url, char *song_name, char *artist, int format, int size)
{
    OPERATE_RET rt = OPRT_OK;
	PR_DEBUG("audio player -> play local url: %s", url);
    AI_PLAYER_SRC_E src = (strstr(url, "http://") == url || strstr(url, "https://") == url) ?
                           AI_PLAYER_SRC_URL : AI_PLAYER_SRC_FILE;
    __music_meta_clear();
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_music_playlist, src, url, format));
    if (song_name || artist) {
        s_music_meta[0].song_name = __dup_str_opt(song_name);
        s_music_meta[0].artist = __dup_str_opt(artist);
        s_music_meta_cnt = 1;
    }

    return rt;
}

/**
@brief Stop all audio players
@param type Player type to stop (foreground, background, or all)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_stop(AI_AUDIO_PLAYER_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("audio player -> stop all player");

    switch (type)
    {
    case AI_AUDIO_PLAYER_FG:
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_tone_player));
        break;
    case AI_AUDIO_PLAYER_BG:
        __music_struct_free(s_pending_music);
        s_pending_music = NULL;
        s_pending_fg_wait_attempt = 0;
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_music_player));
        break;
    case AI_AUDIO_PLAYER_ALL:
        __music_struct_free(s_pending_music);
        s_pending_music = NULL;
        s_pending_fg_wait_attempt = 0;
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_tone_player));
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_music_player));
        break;
    default:
        break;
    }

    return rt;
}

/**
@brief Play alert audio
@param type Alert type
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("audio player -> play alert type=%d", type);

#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)
    __player_local_alert(type);

#elif defined(AI_AGENT_ENABLE_CLOUD_ALERT) && (AI_AGENT_ENABLE_CLOUD_ALERT == 1)
    __player_cloud_alert(type);

#elif defined(AI_PLAYER_ALERT_SOURCE_CUSTOM) && (AI_PLAYER_ALERT_SOURCE_CUSTOM == 1)
    if(__s_alert_custom_cb) {
       rt = __s_alert_custom_cb(type);
    }else {
        PR_ERR("audio player -> alert custom cb is NULL");
        rt = OPRT_NOT_SUPPORTED;
    }

#endif

    return rt;
}

/**
@brief Set audio player volume
@param vol Volume value (0-100)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_vol(int vol)
{
    PR_DEBUG("audio player -> set volume %d", vol);

    return tuya_ai_player_set_volume(__s_tone_player, vol);
}

/**
@brief Get audio player volume
@param vol Pointer to store volume value
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_get_vol(int *vol)
{
    return tuya_ai_player_get_volume(__s_tone_player, vol);
}

/**
 * @brief Register a custom alert callback function.
 *
 * @param cb Pointer to the custom alert callback function. The callback will be
 *           invoked when an alert event occurs, receiving the alert type as parameter.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_audio_player_reg_alert_cb(AI_PLAYER_ALERT_CUSTOM_CB cb)
{
    __s_alert_custom_cb = cb;

    return OPRT_OK;
}
