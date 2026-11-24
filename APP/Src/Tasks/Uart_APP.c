#include "Uart_APP.h"
#include "cmsis_os2.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>

extern osMessageQueueId_t BTTxQueueHandle;
extern osThreadId_t BT_taskHandle;
extern osSemaphoreId_t BT_switchHandle;

uint8_t BT_IsEnabled(void)
{
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET;
}

void BT_SetEnabled(uint8_t enabled)
{
    /* PA8 是 BLE_EN，高电平给蓝牙模块供电。 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (!enabled) osMessageQueueReset(BTTxQueueHandle);
    osThreadFlagsSet(BT_taskHandle, BT_SWITCH_CHANGED);
    /* 通知显示任务重读 BLE_EN。多次切换只需刷新一次最终状态。 */
    osSemaphoreRelease(BT_switchHandle);
}

void Uart_printf(const char *fmt,...)
{
    char buf[UART_TX_ITEM_SIZE];
    va_list args;
    va_start(args,fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    osMessageQueuePut(BTTxQueueHandle, buf, 1, osWaitForever);
}
