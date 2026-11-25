/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include "lvgl.h"
#include "custom.h"
#include "Uart_APP.h"
#include "main.h"
#include "watch_power.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

void BT_SyncControls(gg_ui_t *ui)
{
    uint8_t enabled = BT_IsEnabled();
    if (ui->main.screen != NULL) {
        lv_obj_set_state(ui->main.xiala_button_1_copy, LV_STATE_CHECKED, enabled);
    }
    if (ui->BuleTooth.screen != NULL) {
        lv_obj_set_state(ui->BuleTooth.container_1_switch_1, LV_STATE_CHECKED, enabled);
        lv_label_set_text_static(ui->BuleTooth.label_2, enabled ? "Bluetooth ON" : "Bluetooth OFF");
    }
}

static void bt_control_changed(lv_event_t *event)
{
    lv_obj_t *control = lv_event_get_current_target_obj(event);
    BT_SetEnabled(lv_obj_has_state(control, LV_STATE_CHECKED));
}

void BT_BindControl(gg_ui_t *ui, lv_obj_t *control)
{
    /* 页面重建时，即使之前的通知已消费，新控件也会显示当前状态。 */
    BT_SyncControls(ui);
    lv_obj_add_event_cb(control, bt_control_changed, LV_EVENT_VALUE_CHANGED, NULL);
}


void CL_Power(lv_event_t *event)
{
    (void)event;
    HAL_GPIO_WritePin(POWER_EN_GPIO_Port, POWER_EN_Pin, GPIO_PIN_RESET);
    Watch_Power_Sleep();
}

void custom_init(gg_ui_t *ui)
{
    /* Add your codes here */
}

