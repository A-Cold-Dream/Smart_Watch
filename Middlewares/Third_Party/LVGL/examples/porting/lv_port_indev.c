#include "lv_port_indev.h"
#include "CST816.h"
#include "lcd_init.h"
#include "template_config.h"

volatile int32_t touch_cal_x_ofs = TEMPLATE_TOUCH_X_OFFSET;
volatile int32_t touch_cal_y_ofs = TEMPLATE_TOUCH_Y_OFFSET;

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int32_t last_x;
    static int32_t last_y;
    CST816_Info point;
    int32_t x;
    int32_t y;
    (void)indev;

    if(CST816_ReadTouch(&point)) {
        x = (int32_t)point.X_Pos + touch_cal_x_ofs;
        y = (int32_t)point.Y_Pos + touch_cal_y_ofs;
        if(x < 0) x = 0;
        if(y < 0) y = 0;
        if(x >= LCD_W) x = LCD_W - 1;
        if(y >= LCD_H) y = LCD_H - 1;
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_indev_init(void)
{
    lv_indev_t *indev;
    CST816_GPIO_Init();
    CST816_RESET();
    /* This driver polls, so keep the controller awake while the UI is active. */
    CST816_IIC_WriteREG(DisAutoSleep, 0x01U);
    indev = lv_indev_create();
    lv_indev_set_display(indev, lv_display_get_default());
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
    lv_timer_set_period(lv_indev_get_read_timer(indev), 10U);
}
