#include "WatchTimer.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "watch_display.h"
#include "gui_guider.h"
#include "assets/fonts/gg_font.h"

extern osTimerId_t StopwatchTimerHandle;
extern osTimerId_t CountdownTimerHandle;

/* 计时状态只由 LVGL 任务读写。软件定时器回调只发线程标志。 */
static enum { WATCH_IDLE, 
              WATCH_RUNNING, 
              WATCH_PAUSED } watch_state;
static uint32_t watch_started, watch_elapsed;
static lv_obj_t *watch_screen, *watch_value, *watch_toggle;

static bool countdown_running;
static uint32_t countdown_started, countdown_ticks;
static unsigned selected_seconds = 60U;
static lv_obj_t *countdown_screen, *edit_panel, *minutes, *seconds;
static lv_obj_t *countdown_value, *countdown_toggle;
static lv_timer_t *countdown_refresh;
static gg_ui_t *ui;
static int32_t menu_scroll;

static lv_obj_t *new_screen(const char *title)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101518), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text_static(label, title);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 18);
    return screen;
}

static lv_obj_t *button(lv_obj_t *screen, const char *text, int width, int x, int y,
                        lv_event_cb_t clicked, void *data)
{
    lv_obj_t *key = lv_label_create(screen);
    lv_label_set_text_static(key, text);
    lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(key, width, 40);
    lv_obj_align(key, LV_ALIGN_BOTTOM_MID, x, y);
    lv_obj_set_style_text_align(key, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(key, 10, 0);
    lv_obj_set_style_radius(key, 8, 0);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(key, lv_color_hex(0x204658), 0);
    lv_obj_set_style_bg_color(key, lv_color_hex(0x397795), LV_STATE_PRESSED);
    lv_obj_add_event_cb(key, clicked, LV_EVENT_CLICKED, data);
    return key;
}

static void go_back(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_user_data(event);
    if (!target) {
        /* 重建列表前先腾出滚轮等控件占用的 LVGL 堆。 */
        lv_obj_clean(lv_screen_active());
        if (!ui->right.screen) setup_right(ui);
        target = ui->right.screen;
        lv_obj_update_layout(target);
        lv_obj_scroll_to_y(target, menu_scroll, LV_ANIM_OFF);
    }
    lv_screen_load_anim(target, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static lv_obj_t *open_from_menu(lv_event_t *event, const char *title)
{
    ui = lv_event_get_user_data(event);
    menu_scroll = lv_obj_get_scroll_y(ui->right.screen);
    lv_obj_null_on_delete(&ui->right.screen);
    lv_obj_t *screen = new_screen(title);
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
    return screen;
}

static void show_elapsed(uint32_t ticks)
{
    uint32_t ms = ticks * portTICK_PERIOD_MS;
    lv_label_set_text_fmt(watch_value, "%02u:%02u.%03u", (unsigned)(ms / 60000U),
                          (unsigned)(ms / 1000U % 60U), (unsigned)(ms % 1000U));
}

void Stopwatch_Refresh(void)
{
    if (watch_state == WATCH_RUNNING) {
        show_elapsed(watch_elapsed + (osKernelGetTickCount() - watch_started));
    } else if (osTimerIsRunning(StopwatchTimerHandle)) {
        /* 启动命令尚未执行就离页时，收到最后一次通知后补停。 */
        osTimerStop(StopwatchTimerHandle);
    }
}

static void freeze_watch(void)
{
    if (watch_state == WATCH_RUNNING) {
        watch_elapsed += osKernelGetTickCount() - watch_started;
        osTimerStop(StopwatchTimerHandle);
    }
}

static void toggle_watch(lv_event_t *event)
{
    (void)event;
    if (watch_state == WATCH_RUNNING) {
        freeze_watch();
        watch_state = WATCH_PAUSED;
        lv_label_set_text_static(watch_toggle, "Resume");
    } else {
        uint32_t now = osKernelGetTickCount();
        if (osTimerStart(StopwatchTimerHandle, pdMS_TO_TICKS(20U)) != osOK) {
            lv_label_set_text_static(watch_value, "Timer unavailable");
            return;
        }
        if (watch_state == WATCH_IDLE) watch_elapsed = 0U;
        watch_started = now;
        watch_state = WATCH_RUNNING;
        lv_label_set_text_static(watch_toggle, "Pause");
    }
    show_elapsed(watch_elapsed);
}

static void end_watch(lv_event_t *event)
{
    (void)event;
    freeze_watch();
    watch_state = WATCH_IDLE;
    lv_label_set_text_static(watch_toggle, "Start");
    show_elapsed(watch_elapsed);
}

static void leave_watch(lv_event_t *event)
{
    (void)event;
    freeze_watch();
    watch_state = WATCH_IDLE;
    watch_elapsed = 0U;
}

void Stopwatch_Open(lv_event_t *event)
{
    watch_screen = open_from_menu(event, "Stopwatch");
    lv_obj_null_on_delete(&watch_screen);
    lv_obj_add_event_cb(watch_screen, leave_watch, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    watch_value = lv_label_create(watch_screen);
    lv_obj_set_style_text_font(watch_value, &lv_font_montserratMedium_18, 0);
    lv_obj_align(watch_value, LV_ALIGN_CENTER, 0, -26);
    show_elapsed(0U);
    watch_toggle = button(watch_screen, "Start", 96, -54, -66, toggle_watch, NULL);
    button(watch_screen, "End", 96, 54, -66, end_watch, NULL);
    button(watch_screen, "Back", 204, 0, -14, go_back, NULL);
}

static lv_obj_t *duration_roller(const char *unit, int x, unsigned value, const char *options)
{
    lv_obj_t *label = lv_label_create(edit_panel);
    lv_label_set_text_static(label, unit);
    lv_obj_align(label, LV_ALIGN_TOP_MID, x, 0);
    lv_obj_t *roller = lv_roller_create(edit_panel);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, 74);
    lv_obj_align(roller, LV_ALIGN_BOTTOM_MID, x, 0);
    lv_roller_set_selected(roller, value, LV_ANIM_OFF);
    return roller;
}

static void refresh_remaining(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed = osKernelGetTickCount() - countdown_started;
    uint32_t left = elapsed < countdown_ticks ? countdown_ticks - elapsed : 0U;
    unsigned sec = (left * portTICK_PERIOD_MS + 999U) / 1000U;
    lv_label_set_text_fmt(countdown_value, "%02u:%02u", sec / 60U, sec % 60U);
}

static void countdown_controls(void)
{
    lv_obj_set_flag(edit_panel, LV_OBJ_FLAG_HIDDEN, countdown_running);
    lv_obj_set_flag(countdown_value, LV_OBJ_FLAG_HIDDEN, !countdown_running);
    lv_label_set_text_static(countdown_toggle, countdown_running ? "Stop" : "Start");
    if (countdown_running) {
        refresh_remaining(NULL);
        lv_timer_resume(countdown_refresh);
    } else {
        lv_timer_pause(countdown_refresh);
    }
}

static void toggle_countdown(lv_event_t *event)
{
    (void)event;
    if (countdown_running) {
        /* 到时与点击同时发生时，优先显示到时提醒。 */
        if (osKernelGetTickCount() - countdown_started >= countdown_ticks) return;
        countdown_running = false;
        osTimerStop(CountdownTimerHandle);
    } else {
        selected_seconds = lv_roller_get_selected(minutes) * 60U + lv_roller_get_selected(seconds);
        if (!selected_seconds) return;
        countdown_ticks = pdMS_TO_TICKS(selected_seconds * 1000U);
        countdown_started = osKernelGetTickCount();
        if (osTimerStart(CountdownTimerHandle, countdown_ticks) != osOK) {
            lv_label_set_text_static(countdown_toggle, "Retry");
            return;
        }
        countdown_running = true;
    }
    countdown_controls();
}

static void delete_countdown(lv_event_t *event)
{
    (void)event;
    lv_timer_delete(countdown_refresh);
}

void Countdown_Open(lv_event_t *event)
{
    countdown_screen = open_from_menu(event, "Countdown");
    lv_obj_null_on_delete(&countdown_screen);
    edit_panel = lv_obj_create(countdown_screen);
    lv_obj_remove_style_all(edit_panel);
    lv_obj_remove_flag(edit_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(edit_panel, 210, 108);
    lv_obj_align(edit_panel, LV_ALIGN_CENTER, 0, -15);

    char options[60 * 3];
    for (unsigned i = 0; i < 60U; ++i) {
        options[i * 3U] = '0' + i / 10U;
        options[i * 3U + 1U] = '0' + i % 10U;
        options[i * 3U + 2U] = i == 59U ? '\0' : '\n';
    }
    minutes = duration_roller("Min", -50, selected_seconds / 60U, options);
    seconds = duration_roller("Sec", 50, selected_seconds % 60U, options);
    countdown_value = lv_label_create(countdown_screen);
    lv_obj_set_style_text_font(countdown_value, &lv_font_Alatsi_48, 0);
    lv_obj_align(countdown_value, LV_ALIGN_CENTER, 0, -15);
    button(countdown_screen, "Back", 96, -54, -18, go_back, NULL);
    countdown_toggle = button(countdown_screen, "Start", 96, 54, -18, toggle_countdown, NULL);
    countdown_refresh = lv_timer_create(refresh_remaining, 200U, NULL);
    lv_obj_add_event_cb(countdown_screen, delete_countdown, LV_EVENT_DELETE, NULL);
    countdown_controls();
}

void Countdown_Expired(void)
{
    /* 停止或重新开始后可能还会收到旧通知，使用当前起点复核。 */
    if (!countdown_running || osKernelGetTickCount() - countdown_started < countdown_ticks) return;
    countdown_running = false;
    lv_obj_t *previous = lv_display_get_screen_loading(NULL);
    if (!previous) previous = lv_screen_active();
    bool close_tool = previous == countdown_screen || previous == watch_screen;
    lv_obj_t *alert = new_screen("Countdown");
    lv_obj_t *message = lv_label_create(alert);
    lv_label_set_text_static(message, "Time's up!");
    lv_obj_align(message, LV_ALIGN_CENTER, 0, -12);
    button(alert, "OK", 136, 0, -24, go_back, close_tool ? NULL : previous);
    lv_screen_load_anim(alert, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, close_tool);
    lv_indev_wait_release(lv_indev_get_next(NULL));
    Watch_Display_UpdatePower(true);
}
