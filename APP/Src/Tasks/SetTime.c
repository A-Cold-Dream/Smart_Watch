#include "SetTime.h"
#include "assets/fonts/gg_font.h"

RTC_TimeTypeDef Time;
RTC_DateTypeDef Date;

static lv_obj_t *hour_roller;
static lv_obj_t *minute_roller;
static lv_obj_t *second_roller;
static lv_obj_t *title;

HAL_StatusTypeDef SetTime_Read(void)
{
    HAL_StatusTypeDef status = HAL_RTC_GetTime(&hrtc, &Time, RTC_FORMAT_BIN);
    if (status != HAL_OK) return status;
    /* STM32F4 读完时间后要读日期，才能解锁下一次日历快照。 */
    return HAL_RTC_GetDate(&hrtc, &Date, RTC_FORMAT_BIN);
}

static lv_obj_t *add_roller(lv_obj_t *screen, const char *unit, int x_offset,
                            unsigned maximum, unsigned current)
{
    lv_obj_t *caption = lv_label_create(screen);
    lv_label_set_text_static(caption, unit);
    lv_obj_align(caption, LV_ALIGN_TOP_MID, x_offset, 48);

    /* 每项占两个数字和一个分隔符，最后一项以 '\0' 结尾。 */
    char options[61 * 3];
    for (unsigned value = 0; value <= maximum; ++value) {
        char *row = &options[3 * value];
        lv_snprintf(row, 3, "%02u", value);
        if (value < maximum) row[2] = '\n';
    }

    lv_obj_t *roller = lv_roller_create(screen);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(roller, 64);
    lv_obj_set_style_text_font(roller, &lv_font_montserratMedium_18, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(roller, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x18252C), LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(0xA6B8C0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x20536A), LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_align(roller, LV_ALIGN_TOP_MID, x_offset, 76);
    lv_roller_set_selected(roller, current, LV_ANIM_OFF);
    return roller;
}

static void close_editor(lv_event_t *event)
{
    lv_obj_t *settings = lv_event_get_user_data(event);
    lv_screen_load_anim(settings, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static void save_time(lv_event_t *event)
{
    /* 60 秒进一分，60 分进一小时，24 时回到 00 时；只修改时间。 */
    uint32_t seconds = lv_roller_get_selected(second_roller);
    uint32_t minutes = lv_roller_get_selected(minute_roller) + seconds / 60U;
    uint32_t hours = lv_roller_get_selected(hour_roller) + minutes / 60U;
    RTC_TimeTypeDef edited = {
        .Hours = hours % 24U,
        .Minutes = minutes % 60U,
        .Seconds = seconds % 60U,
    };
    if (HAL_RTC_SetTime(&hrtc, &edited, RTC_FORMAT_BIN) != HAL_OK) {
        lv_label_set_text_static(title, "Could not save time");
        return;
    }
    close_editor(event);
}

static lv_obj_t *add_button(lv_obj_t *screen, int x, int y, int width,
                            const char *text, lv_event_cb_t clicked, void *data)
{
    /* A clickable label gives each key a full touch area with a single LVGL object. */
    lv_obj_t *button = lv_label_create(screen);
    lv_label_set_text_static(button, text);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(button, width, 40);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_text_align(button, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(button, 10, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x204658), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x397795), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, data);
    return button;
}

void SetTime_Open(lv_event_t *event)
{
    lv_obj_t *settings = lv_obj_get_screen(lv_event_get_current_target_obj(event));
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101518), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    title = lv_label_create(screen);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
    add_button(screen, 14, 184, 96, "Cancel", close_editor, settings);

    if (SetTime_Read() == HAL_OK) {
        lv_label_set_text_static(title, "Set time");
        hour_roller = add_roller(screen, "Hour", -76, 24, Time.Hours);
        minute_roller = add_roller(screen, "Min", 0, 60, Time.Minutes);
        second_roller = add_roller(screen, "Sec", 76, 60, Time.Seconds);
        add_button(screen, 130, 184, 96, "Save", save_time, settings);
    } else {
        lv_label_set_text_static(title, "Could not read time");
    }
    lv_screen_load(screen);
}

