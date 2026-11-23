#include "watch_display.h"

#include "AHT21.h"
#include "CST816.h"
#include "FreeRTOS.h"
#include "MPU6050.h"
#include "SetTime.h"
#include "src/misc/lv_timer.h"
#include "task.h"
#include "custom.h"
#include "gg_utils.h"
#include "lcd.h"
#include "lcd_init.h"
#include "main.h"

/* LVGL 触点纵向校准，单位为像素。负值向上，正值向下。 */
#define TOUCH_Y_OFFSET_PX (-15)

gg_ui_t guider_ui;
static _Alignas(LV_DRAW_BUF_ALIGN) uint16_t draw_buffer[LCD_W * 8];

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
    if (CST816_ReadTouch(&touch) && touch.X_Pos < LCD_W && touch.Y_Pos < raw_y_limit) {
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

static void refresh_TimeDeta(lv_timer_t *timer)
{
    gg_main_t *page = lv_timer_get_user_data(timer);
    if(page ->screen == NULL || page ->screen != lv_screen_active())  return;

    if (SetTime_Read() == HAL_OK) {
        lv_label_set_text_fmt(page->clock, "%02u:%02u", (unsigned)Time.Hours, (unsigned)Time.Minutes);
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

    lv_indev_t *touch = lv_indev_create();
    if (touch == NULL) Error_Handler();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch, display);
    lv_indev_set_read_cb(touch, read_touch);

    setup_ui(&guider_ui);
    custom_init(&guider_ui);

    lv_timer_create(refresh_temp_humidity, 1000U, &guider_ui.Temp_Humidity);
    lv_timer_create(refresh_MPU6050, 200U, &guider_ui.Attitude_Solving);
    lv_timer_t *clock_timer = lv_timer_create(refresh_TimeDeta, 1000U, &guider_ui.main);
    refresh_TimeDeta(clock_timer);

    lv_refr_now(display);
    LCD_Set_Light(60);
    LCD_Open_Light();
}
