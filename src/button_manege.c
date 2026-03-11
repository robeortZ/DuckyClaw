#include "button_manege.h"
#include "tdl_button_manage.h"
#include "tal_api.h"

#if defined(BUTTON_NAME_2) || defined(BUTTON_NAME_3)
#include "ai_chat_main.h"
#include "tkl_gpio.h"
#include "tuya_cloud_types.h"
#include <string.h>
#endif

#define VOLUME_STEP  5
#define VOLUME_MIN   0
#define VOLUME_MAX   100

#if defined(BUTTON_NAME_2) || defined(BUTTON_NAME_3)
static TDL_BUTTON_HANDLE sg_button_hdl_2 = NULL;
static TDL_BUTTON_HANDLE sg_button_hdl_3 = NULL;

static void __volume_button_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *arg)
{
    if (event == TDL_BUTTON_PRESS_DOUBLE_CLICK) {
        //power_on_off();
        tkl_gpio_write(TUYA_GPIO_NUM_9, TUYA_GPIO_LEVEL_LOW);
        PR_NOTICE("power off");
        return;
    }

    int vol = ai_chat_get_volume();
#if defined(BUTTON_NAME_2)
    if (name && strcmp(name, BUTTON_NAME_2) == 0) {
        vol += VOLUME_STEP;
        if (vol > VOLUME_MAX) {
            vol = VOLUME_MAX;
        }
        ai_chat_set_volume(vol);
        return;
    }
#endif
#if defined(BUTTON_NAME_3)
    if (name && strcmp(name, BUTTON_NAME_3) == 0) {
        vol -= VOLUME_STEP;
        if (vol < VOLUME_MIN) {
            vol = VOLUME_MIN;
        }
        ai_chat_set_volume(vol);
        return;
    }
#endif
}
#endif

OPERATE_RET voice_manager_buttons_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    tdl_button_set_task_stack_size(4096);

    TDL_BUTTON_CFG_T button_cfg = {
        .long_start_valid_time = 400,
        .long_keep_timer = 0,
        .button_debounce_time = 50,
        .button_repeat_valid_count = 2,
        .button_repeat_valid_time = 300,
    };

#if defined(BUTTON_NAME_2)
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &button_cfg, &sg_button_hdl_2));
    tdl_button_event_register(sg_button_hdl_2, TDL_BUTTON_PRESS_SINGLE_CLICK, __volume_button_cb);
    tdl_button_event_register(sg_button_hdl_2, TDL_BUTTON_PRESS_DOUBLE_CLICK, __volume_button_cb);
#endif

#if defined(BUTTON_NAME_3)
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_3, &button_cfg, &sg_button_hdl_3));
    tdl_button_event_register(sg_button_hdl_3, TDL_BUTTON_PRESS_SINGLE_CLICK, __volume_button_cb);
    tdl_button_event_register(sg_button_hdl_3, TDL_BUTTON_PRESS_DOUBLE_CLICK, __volume_button_cb);
#endif

    return rt;
}