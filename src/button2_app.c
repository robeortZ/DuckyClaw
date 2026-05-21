#include "tal_api.h"
#include "tkl_kws.h"
#include "tuya_iot.h"

#include "cJSON.h"
#include "tuya_ai_agent.h"

#include "tdl_button_manage.h"
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;

//BUTTON_NAME_2

static void __button2_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    PR_DEBUG("button2 event: %d", event);

    if(TDL_BUTTON_PRESS_DOUBLE_CLICK == event) {
        PR_DEBUG("button2 double click");
        tuya_iot_reset(tuya_iot_client_get());
    }
    if(TDL_BUTTON_LONG_PRESS_START == event) {
        PR_DEBUG("button2 long press start");
        tal_system_reset();
    }
}
OPERATE_RET __button2_app_open_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("button2 create");
    tdl_button_set_task_stack_size(4096);

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 400,
                                   .long_keep_timer = 0,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 300};
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __button2_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __button2_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __button2_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __button2_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_LONG_PRESS_START, __button2_function_cb);

    PR_DEBUG("button2 event registered successfully");
    return rt;
}
