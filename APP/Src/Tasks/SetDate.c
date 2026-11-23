#include "SetDate.h"
#include "SetTime.h"

/* STM32 RTC 用两位数字保存 2000 至 2099 年。 */
static unsigned year;

static void update_days(gg_SetData_t *page, unsigned day)
{
    static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned month = lv_roller_get_selected(page->Mon);
    unsigned count = month_days[month];
    if (month == 1 && year % 4 == 0) ++count;

    if (lv_roller_get_option_count(page->day) != count) {
        char options[31 * 3];
        size_t used = 0;
        for (unsigned d = 1; d <= count; ++d) {
            used += lv_snprintf(options + used, sizeof(options) - used, d == 1 ? "%u" : "\n%u", d);
        }
        lv_roller_set_options(page->day, options, LV_ROLLER_MODE_NORMAL);
    }
    /* 切到较短的月份时，保留到该月的最后一天。 */
    lv_roller_set_selected(page->day, LV_MIN(day, count) - 1U, LV_ANIM_OFF);
}

static void load_date(lv_event_t *event)
{
    gg_SetData_t *page = lv_event_get_user_data(event);
    if (SetTime_Read() != HAL_OK) {
        lv_obj_add_state(page->Save, LV_STATE_DISABLED);
        lv_label_set_text_static(page->Top, "Read error");
        return;
    }
    lv_obj_remove_state(page->Save, LV_STATE_DISABLED);
    lv_label_set_text_static(page->Top, "Set Date");
    year = 2000U + Date.Year;
    lv_label_set_text_fmt(page->year, "%u", year);
    lv_roller_set_selected(page->Mon, Date.Month - 1U, LV_ANIM_OFF);
    lv_roller_set_selected(page->week, Date.WeekDay - 1U, LV_ANIM_OFF);
    update_days(page, Date.Date);
}

static void change_calendar(lv_event_t *event)
{
    gg_SetData_t *page = lv_event_get_user_data(event);
    lv_obj_t *control = lv_event_get_current_target_obj(event);
    if (control == page->Plus && year < 2099U) ++year;
    if (control == page->Minus && year > 2000U) --year;
    lv_label_set_text_fmt(page->year, "%u", year);
    update_days(page, lv_roller_get_selected(page->day) + 1U);
}

void SetDate_Bind(gg_SetData_t *page)
{
    lv_obj_add_event_cb(page->screen, load_date, LV_EVENT_SCREEN_LOAD_START, page);
    lv_obj_add_event_cb(page->Mon, change_calendar, LV_EVENT_VALUE_CHANGED, page);
    lv_obj_add_event_cb(page->Plus, change_calendar, LV_EVENT_CLICKED, page);
    lv_obj_add_event_cb(page->Minus, change_calendar, LV_EVENT_CLICKED, page);
}

bool SetDate_Save(gg_SetData_t *page)
{
    RTC_DateTypeDef edited = {
        .Year = year - 2000U,
        .Month = lv_roller_get_selected(page->Mon) + 1U,
        .Date = lv_roller_get_selected(page->day) + 1U,
        .WeekDay = lv_roller_get_selected(page->week) + 1U,
    };
    if (HAL_RTC_SetDate(&hrtc, &edited, RTC_FORMAT_BIN) == HAL_OK) return true;
    lv_label_set_text_static(page->Top, "Save error");
    return false;
}
