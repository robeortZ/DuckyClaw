#include "tal_api.h"

#include <string.h>

#if defined(ENABLE_AI_CHAT_CUSTOM_UI) && (ENABLE_AI_CHAT_CUSTOM_UI == 1)
#include "lvgl.h"
#include "lv_vendor.h"
#include "tal_time_service.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "cron_service.h"

#include "ducky_custom_ui.h"
#include "screen_manager.h"

static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024 * 8);
}

OPERATE_RET ducky_custom_ui_register(void)
{
    OPERATE_RET    rt = OPRT_OK;
    AI_UI_INTFS_T intfs;

    __lvgl_init();
    memset(&intfs, 0, sizeof(intfs));

    intfs.disp_init = screens_init;

    TUYA_CALL_ERR_RETURN(ai_ui_register(&intfs));

    return rt;
}

#else

#include "ducky_custom_ui.h"

/**
 * @brief Stub when custom UI is disabled in Kconfig
 * @return OPRT_OK
 */
OPERATE_RET ducky_custom_ui_register(void)
{
    return OPRT_OK;
}

#endif
