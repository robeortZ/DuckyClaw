/**
 * @file screen_manager.c
 * @brief Implementation of screen manager for handling screen navigation and stack operations
 *
 * This file contains the implementation of a stack-based screen navigation system.
 * It provides functions to push screens onto the stack, pop screens from the stack,
 * and navigate between screens with animation effects.
 *
 * The implementation includes:
 * - Stack operations (init, push, pop, check if empty)
 * - Screen navigation functions (back, back to home)
 * - Screen loading with animation
 * - Screen lifecycle management
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "screen_manager.h"
#include "src/display/lv_display.h"
#include "startup_screen.h"
#include "ducky_ui_strings.h"
#include "lv_vendor.h"
// #include "main_screen.h"
#include <stdio.h>

/* Slide duration for screen_load / screen_back (incl. home -> analog clock). Change here for all. */
#ifndef DUCKY_SCREEN_TRANS_MS
#define DUCKY_SCREEN_TRANS_MS 500
#endif

static void __on_screen_unloaded(lv_event_t *e)
{
    Screen_t *s = (Screen_t *)lv_event_get_user_data(e);
    if (s && s->deinit) {
        s->deinit();
    }
}

static void __register_unloaded_cleanup(Screen_t *scr)
{
    if (!scr || !scr->screen_obj || !*scr->screen_obj) {
        return;
    }
    lv_obj_add_event_cb(*scr->screen_obj, __on_screen_unloaded, LV_EVENT_SCREEN_UNLOADED, scr);
}

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_DEPTH 6
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    Screen_t *screens[MAX_DEPTH]; /**< Array of screen pointers */
    uint8_t top;                  /**< Index of the top of the stack */
} ScreenStack_t;
/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static ScreenStack_t screen_stack;

/***********************************************************
***********************function define**********************
***********************************************************/

static void screen_stack_init(ScreenStack_t *stack)
{
    stack->top = 0;
}

static uint8_t screen_stack_push(ScreenStack_t *stack, Screen_t *screen)
{
    if (stack->top >= MAX_DEPTH)
        return -1;
    stack->screens[stack->top++] = screen;
    return 0;
}

static uint8_t screen_stack_is_empty(const ScreenStack_t *stack)
{
    return stack->top == 0;
}

static Screen_t *get_top_screen(ScreenStack_t *stack)
{
    // Check if stack is empty
    if (stack->top == 0) {
        return NULL; // Return NULL if stack is empty
    }

    // Return pointer to top screen
    return stack->screens[stack->top - 1];
}

/**
 * @brief Get the current screen (top of stack)
 * @return Pointer to the current screen, or NULL if the screen stack is empty
 */
Screen_t *screen_get_now_screen(void)
{
    return get_top_screen(&screen_stack);
}

/**
 * @brief Go back to the previous screen
 *
 * This function unloads the current screen and loads the previous screen.
 * If there is no previous screen, it loads the startup screen.
 */
void screen_back(void)
{
    if (screen_stack_is_empty(&screen_stack) || screen_stack.top <= 1) {
        return;
    }

    lv_vendor_disp_lock();

    Screen_t *current  = screen_stack.screens[screen_stack.top - 1];
    Screen_t *previous = screen_stack.screens[screen_stack.top - 2];

    __register_unloaded_cleanup(current);
    previous->init();
    if (previous->screen_obj && *previous->screen_obj) {
        lv_screen_load_anim(*previous->screen_obj, LV_SCR_LOAD_ANIM_OUT_RIGHT, DUCKY_SCREEN_TRANS_MS, 0,
                             true);
        printf("[Manager] Returning to previous screen: %s\n", previous->name);
    } else {
        printf("[Error] %s is NULL or invalid\n", previous->name);
    }

    screen_stack.top--;

    lv_vendor_disp_unlock();
}

/**
 * @brief Go back to the previous screen with a chosen LVGL screen transition
 * @param[in] anim lv_screen_load_anim_t (e.g. LV_SCR_LOAD_ANIM_OUT_RIGHT for pull-down notices)
 */
void screen_back_anim(lv_screen_load_anim_t anim)
{
    if (screen_stack_is_empty(&screen_stack) || screen_stack.top <= 1) {
        return;
    }

    lv_vendor_disp_lock();

    Screen_t *current  = screen_stack.screens[screen_stack.top - 1];
    Screen_t *previous = screen_stack.screens[screen_stack.top - 2];

    __register_unloaded_cleanup(current);
    previous->init();
    if (previous->screen_obj && *previous->screen_obj) {
        lv_screen_load_anim(*previous->screen_obj, anim, DUCKY_SCREEN_TRANS_MS, 0, true);
        printf("[Manager] Returning to previous screen: %s\n", previous->name);
    } else {
        printf("[Error] %s is NULL or invalid\n", previous->name);
    }

    screen_stack.top--;

    lv_vendor_disp_unlock();
}

/**
 * @brief Go back to the home screen (bottom of stack)
 *
 * This function unloads all screens except the home screen.
 */
void screen_back_bottom(void)
{
    if (screen_stack_is_empty(&screen_stack) || screen_stack.top <= 1) {
        return;
    }

    lv_vendor_disp_lock();

    while (screen_stack.top > 1) {
        Screen_t *current = screen_stack.screens[screen_stack.top - 1];
        Screen_t *below   = screen_stack.screens[screen_stack.top - 2];

        printf("[%s] pop screen\n", current->name);
        __register_unloaded_cleanup(current);
        below->init();
        if (below->screen_obj && *below->screen_obj) {
            lv_screen_load_anim(*below->screen_obj, LV_SCR_LOAD_ANIM_FADE_IN, DUCKY_SCREEN_TRANS_MS, 0,
                                true);
        }
        screen_stack.top--;
    }

    printf("[Manager] Returning to home screen: [%s]\n", screen_stack.screens[0]->name);

    lv_vendor_disp_unlock();
}

void screen_load_anim(Screen_t *newScreen, lv_screen_load_anim_t anim)
{
    /* Check if stack is full */
    if (screen_stack.top >= MAX_DEPTH - 1) {
        printf("[Manager] screen stack full, refuse load\n");
        return;
    }

    lv_vendor_disp_lock();

    /*
     * Never deinit/delete the current active screen before the new one is created
     * and lv_screen_load* has switched away — otherwise LVGL faults (MemFault on lvgl thread).
     */
    Screen_t *old_top = NULL;
    if (screen_stack.top > 0) {
        old_top = screen_stack.screens[screen_stack.top - 1];
    }

    if (old_top) {
        __register_unloaded_cleanup(old_top);
    }

    screen_stack_push(&screen_stack, newScreen);
    newScreen->init();

    if (newScreen->screen_obj && *newScreen->screen_obj) {
        lv_screen_load_anim(*newScreen->screen_obj, anim, DUCKY_SCREEN_TRANS_MS, 0, true);
        printf("[Manager] Loading to new screen: [%s]\n", newScreen->name);
    } else {
        printf("[Error] %s is NULL or invalid\n", newScreen->name);
    }

    lv_vendor_disp_unlock();
}

/**
 * @brief Load a new screen to the top of the stack
 *
 * @param newScreen Pointer to the new screen to be loaded
 */
void screen_load(Screen_t *newScreen)
{
    screen_load_anim(newScreen, LV_SCR_LOAD_ANIM_OUT_LEFT);
}

/**
 * @brief Initialize the screen manager
 *
 * This function initializes the screen stack and loads the startup screen.
 */
 OPERATE_RET screens_init(void)
{
    PR_DEBUG("screens_init");
    ducky_ui_lang_init();
    /*
     * disp_init runs on the ai_ui init thread while the lvgl task runs lv_task_handler()
     * under the same mutex. All LVGL calls must be wrapped like ai_ui_chat_* does.
     */
    lv_vendor_disp_lock();
    screen_stack_init(&screen_stack);
    screen_stack_push(&screen_stack, &startup_screen);
    startup_screen.init();

    // Check if screen object is valid before loading
    if (startup_screen.screen_obj && *startup_screen.screen_obj) {
        lv_disp_load_scr(*startup_screen.screen_obj);
    } else {
        printf("[Error]: startup_screen.screen_obj is NULL or invalid during initialization\n");
    }
    lv_vendor_disp_unlock();

    PR_DEBUG("screens_init success");
    return OPRT_OK;
}
