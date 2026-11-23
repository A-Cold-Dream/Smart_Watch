#include "lv_port_disp.h"
#include "lcd.h"
#include "lcd_init.h"
#include "template_config.h"

#define DISPLAY_BUFFER_BYTES (LCD_W * TEMPLATE_DISPLAY_BUFFER_LINES * 2U)
#if DISPLAY_BUFFER_BYTES > 65535U
#error "Display buffer exceeds the HAL SPI transfer length"
#endif

LV_ATTRIBUTE_MEM_ALIGN static uint8_t display_buffer_1[DISPLAY_BUFFER_BYTES];
LV_ATTRIBUTE_MEM_ALIGN static uint8_t display_buffer_2[DISPLAY_BUFFER_BYTES];

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    LCD_Set_Window_Raw((u16)area->x1,
                       (u16)(area->y1 + TEMPLATE_LCD_GRAM_Y_OFFSET),
                       (u16)area->x2,
                       (u16)(area->y2 + TEMPLATE_LCD_GRAM_Y_OFFSET));
    LCD_Write_Pixels(px_map, width * height * 2U);
    lv_display_flush_ready(disp);
}

void lv_port_disp_init(void)
{
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(disp, display_buffer_1, display_buffer_2,
                           sizeof(display_buffer_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}
