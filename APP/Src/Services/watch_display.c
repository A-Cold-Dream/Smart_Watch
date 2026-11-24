#include "watch_display.h"

#include "AHT21.h"
#include "CST816.h"
#include "EM7028.h"
#include "FreeRTOS.h"
#include "LSM303DLHC.h"
#include "MPU6050.h"
#include "SPL06_001.h"
#include "SetTime.h"
#include "gui_guider.h"
#include "src/display/lv_display.h"
#include "src/misc/lv_timer.h"
#include "src/misc/lv_types.h"
#include "src/widgets/label/lv_label.h"
#include "src/widgets/slider/lv_slider.h"
#include "task.h"
#include "custom.h"
#include "gg_utils.h"
#include "lcd.h"
#include "lcd_init.h"
#include "main.h"
#include <stdint.h>
#include <sys/types.h>

/* LVGL 触点纵向校准，单位为像素。负值向上，正值向下。 */
#define TOUCH_Y_OFFSET_PX (-15)
#define SCREEN_IDLE_TIMEOUT_MS 15000U

gg_ui_t guider_ui;
static _Alignas(LV_DRAW_BUF_ALIGN) uint16_t draw_buffer[LCD_W * 8];
volatile uint8_t LCD_Light = 50;
static bool screen_off;
static lv_indev_t *touch_input;

static void flush_display(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint32_t count = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(pixels, count);
    LCD_Set_Window(area->x1, area->y1, area->x2, area->y2);
    LCD_Write_Pixels(pixels, count * sizeof(uint16_t));
    lv_display_flush_ready(display);
}

static void read_touch(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_point_t last_point;
    CST816_Info touch;
    (void)indev;

    data->state = LV_INDEV_STATE_RELEASED;
    /* Bottom-edge samples can be outside LCD_H before the upward correction. */
    const unsigned int raw_y_limit = LCD_H - LV_MIN(TOUCH_Y_OFFSET_PX, 0);
    if (!screen_off && CST816_ReadTouch(&touch) && touch.X_Pos < LCD_W && touch.Y_Pos < raw_y_limit) {
        int32_t y = (int32_t)touch.Y_Pos + TOUCH_Y_OFFSET_PX;
        last_point.x = touch.X_Pos;
        last_point.y = LV_CLAMP(0, y, LCD_H - 1);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    data->point = last_point;
}

static void refresh_temp_humidity(lv_timer_t *timer)
{
    gg_Temp_Humidity_t *page = lv_timer_get_user_data(timer);
    /* GUI Guider 离开页面时会删除控件，重新进入时会创建新的 label_2。 */
    if (page->screen == NULL || page->screen != lv_screen_active()) return;

    /* 一次取走同一帧的温度和湿度，退出临界区后再更新界面。 */
    taskENTER_CRITICAL();
    AHT21_Sample_t sample = AHT21_Sample;
    taskEXIT_CRITICAL();

    if (!sample.valid) {
        lv_label_set_text_static(page->label_2, "Temp:  --.-°\n\nHumidity:  --.-%");
        return;
    }

    /* 底驱给出十分位整数。单独处理负号，-5 也能显示为 -0.5°。 */
    int temp = sample.temperature_x10;
    int magnitude = temp < 0 ? -temp : temp;
    unsigned int humidity = sample.humidity_x10;
    lv_label_set_text_fmt(page->label_2, "Temp:  %s%d.%d°\n\nHumidity:  %u.%u%%",
                          temp < 0 ? "-" : "", magnitude / 10, magnitude % 10,
                          humidity / 10U, humidity % 10U);
}

static void refresh_MPU6050(lv_timer_t *timer)
{
    gg_Attitude_Solving_t *page = lv_timer_get_user_data(timer);
    if(page ->screen == NULL || page ->screen != lv_screen_active())  return;

    taskENTER_CRITICAL();
    MPU6050_Sample_t sample = MPU6050_Sample;
    taskEXIT_CRITICAL();

    if(!sample.valid)
    {
        lv_label_set_text_static(page ->label_2_copy,
            "Xacc = xx\nYacc = xx\nZacc = xx\n"
            "Xgyro = xx\nYgyro = xx\nZgyro = xx"
            );
            return;
    }

    lv_label_set_text_fmt(page ->label_2_copy,
            "Xacc = %d\nYacc = %d\nZacc = %d\n"
            "Xgyro = %d\nYgyro = %d\nZgyro = %d",
            sample.accel[0],sample.accel[1],sample.accel[2],
            sample.gyro[0],sample.gyro[1],sample.gyro[2]);

}

static void format_axes(char *text, size_t capacity, const int16_t axes[3], uint8_t valid)
{
    if (valid)
        lv_snprintf(text, capacity, "X: %d\nY: %d\nZ: %d", axes[0], axes[1], axes[2]);
    else
        lv_snprintf(text, capacity, "X: --\nY: --\nZ: --");
}

static void refresh_sensor_pages(lv_timer_t *timer)
{
    gg_ui_t *ui = lv_timer_get_user_data(timer);
    lv_obj_t *screen = lv_screen_active();
    if (!screen) return;
    /* GUI Guider 把所属页面的 screen 成员地址存入 user_data，页面重建后仍能识别。 */
    const void *page = lv_obj_get_user_data(screen);
    if (page == &ui->Barometer_Measure.screen) {
        taskENTER_CRITICAL();
        SPL06_001_Sample_t sample = SPL06_001_Sample;
        taskEXIT_CRITICAL();
        lv_obj_t *label = ui->Barometer_Measure.label_2_copy;
        if (sample.valid) {
            int pa = sample.pressure_pa;
            lv_label_set_text_fmt(label, "Pressure\n%d.%02d hPa", pa / 100, pa % 100);
        } else {
            lv_label_set_text_static(label, "Pressure\n-- hPa");
        }
    } else if (page == &ui->Magnetic_Measure.screen) {
        taskENTER_CRITICAL();
        LSM303DLHC_Sample_t sample = LSM303DLHC_Sample;
        taskEXIT_CRITICAL();
        char mag[40], accel[40];
        format_axes(mag, sizeof(mag), sample.mag, sample.mag_valid);
        format_axes(accel, sizeof(accel), sample.accel, sample.accel_valid);
        lv_label_set_text_fmt(ui->Magnetic_Measure.label_2_copy, "Mag raw\n%s\nAccel mg\n%s", mag, accel);
    } else if (page == &ui->Heart_Rate_Measure.screen) {
        taskENTER_CRITICAL();
        EM7028_Sample_t sample = EM7028_Sample;
        taskEXIT_CRITICAL();
        lv_obj_t *label = ui->Heart_Rate_Measure.label_2_copy;
        if (sample.valid)
            lv_label_set_text_fmt(label, "Optical raw\n%u", (unsigned)sample.optical_raw);
        else
            lv_label_set_text_static(label, "Optical raw\n--");
    }
}

static void refresh_TimeDeta(lv_timer_t *timer)
{
    gg_main_t *page = lv_timer_get_user_data(timer);
    if(page ->screen == NULL || page ->screen != lv_screen_active())  return;

    if (SetTime_Read() == HAL_OK) {
        lv_label_set_text_fmt(page->clock, "%02u:%02u", (unsigned)Time.Hours, (unsigned)Time.Minutes);
        static const char *const weekdays[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
        if (Date.WeekDay >= 1U && Date.WeekDay <= 7U) {
            char date_text[16];
            lv_snprintf(date_text, sizeof(date_text), "%02u-%02u  %s",
                        (unsigned)Date.Month, (unsigned)Date.Date, weekdays[Date.WeekDay - 1U]);
            if (lv_strcmp(lv_label_get_text(page->riqi), date_text) != 0)
                lv_label_set_text(page->riqi, date_text);
        }
    }
}

static void refresh_main_TempAndHum(lv_timer_t *timer)
{
    gg_main_t *page = lv_timer_get_user_data(timer);
    if(page ->screen == NULL || page ->screen != lv_screen_active())  return;

    if(!AHT21_Sample.valid)  return;

    int temp = AHT21_Sample.temperature_x10;
    int magnitude = temp < 0 ? -temp : temp;
    unsigned int humidity = AHT21_Sample.humidity_x10;
    
    lv_label_set_text_fmt(page ->wendu_label_4, "%s%d°",temp < 0 ? "-" : "", magnitude / 10);
    lv_label_set_text_fmt(page ->shidu_label_4, "%d%%",humidity/10);
    lv_arc_set_value(page ->wendu,magnitude / 10);//ui->main.wendu, 26
    lv_arc_set_value(page ->shidu,humidity/10);
}

static void refresh_LCD_Light(lv_timer_t *timer)
{
    gg_main_t *page = lv_timer_get_user_data(timer);
    if (screen_off || page->screen == NULL || page->screen != lv_screen_active()) return;
    int32_t brightness = lv_slider_get_value(page->xiala_slider_1);
    if (brightness >= 5 && brightness <= 100) {
        LCD_Light = brightness;
        LCD_Set_Light(LCD_Light);
    }
}

/* 只从 LVGL 任务调用。触摸按下时 LVGL 会自行更新活动时间。 */
void Watch_Display_UpdatePower(bool key1_activity)
{
    lv_display_t *display = lv_display_get_default();
    if (key1_activity) {
        lv_display_trigger_activity(display);
        if (screen_off) {
            lv_indev_wait_release(touch_input);
            LCD_WR_REG(0x29);  /* ST7789 DISPON */
            LCD_Set_Light(LCD_Light);
            screen_off = false;
        }
    } else if (!screen_off && lv_display_get_inactive_time(display) >= SCREEN_IDLE_TIMEOUT_MS) {
        LCD_Close_Light();
        LCD_WR_REG(0x28);  /* ST7789 DISPOFF */
        screen_off = true;
    }
}

void Watch_Display_Init(void)
{
    LCD_Init();
    CST816_Init();
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    lv_display_t *display = lv_display_create(LCD_W, LCD_H);
    if (display == NULL) Error_Handler();
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_display);

    touch_input = lv_indev_create();
    if (touch_input == NULL) Error_Handler();
    lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_input, display);
    lv_indev_set_read_cb(touch_input, read_touch);

    setup_ui(&guider_ui);
    custom_init(&guider_ui);

    lv_timer_create(refresh_temp_humidity, 1000U, &guider_ui.Temp_Humidity);
    lv_timer_create(refresh_MPU6050, 200U, &guider_ui.Attitude_Solving);
    lv_timer_create(refresh_sensor_pages, 200U, &guider_ui);
    lv_timer_t *clock_timer = lv_timer_create(refresh_TimeDeta, 1000U, &guider_ui.main);
    refresh_TimeDeta(clock_timer);
    lv_timer_create(refresh_main_TempAndHum, 1000U, &guider_ui.main);
    lv_timer_create(refresh_LCD_Light, 500U, &guider_ui.main);

    lv_refr_now(display);
    LCD_Set_Light(LCD_Light);
    LCD_Open_Light();
    lv_display_trigger_activity(display);
}
