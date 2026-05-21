/**
 * @file ui_layout.h
 * @brief Round display content insets (avoid bezel clipping)
 */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "lvgl.h"

#ifndef DUCKY_ROUND_PAD_X
#define DUCKY_ROUND_PAD_X 28
#endif

#ifndef DUCKY_ROUND_PAD_Y
#define DUCKY_ROUND_PAD_Y 22
#endif

/** Content width inside circular safe area */
#define DUCKY_CONTENT_W ((lv_coord_t)(LV_HOR_RES - 2 * (DUCKY_ROUND_PAD_X)))

#endif /* UI_LAYOUT_H */
