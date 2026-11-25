#include "watch_power.h"
#include "WatchTimer.h"
#include "Uart_APP.h"
#include "CST816.h"
#include "EM7028.h"
#include "LSM303DLHC.h"
#include "MPU6050.h"
#include "SPL06_001.h"
#include "lcd_init.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

extern osMutexId_t UartMutexHandle;
extern void SystemClock_Config(void);

static volatile bool preparing_sleep, key_wake, alarm_wake;
static volatile unsigned samples_in_progress;

bool Watch_Power_BeginSample(void)
{
    taskENTER_CRITICAL();
    bool allowed = !preparing_sleep;
    if (allowed) ++samples_in_progress;
    taskEXIT_CRITICAL();
    return allowed;
}

void Watch_Power_EndSample(void)
{
    taskENTER_CRITICAL();
    --samples_in_progress;
    taskEXIT_CRITICAL();
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_5) key_wake = true;
}

/* Alarm A 仅在有倒计时的 STOP 期间启用。中断里不访问 LVGL 或 RTOS。 */
void RTC_Alarm_IRQHandler(void)
{
    HAL_RTC_AlarmIRQHandler(&hrtc);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc)
{
    (void)rtc;
    alarm_wake = true;
}

/* 只在倒计时运行时用 RTC 计算睡眠时间，最长 59:59，可跨午夜。 */
static uint32_t rtc_position(uint32_t scale)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN); /* 解锁本次日历快照。 */
    uint32_t sec = (time.Hours * 60U + time.Minutes) * 60U + time.Seconds;
    return sec * scale + (scale - 1U - time.SubSeconds);
}

static bool arm_alarm(uint32_t target, uint32_t scale)
{
    uint32_t sec = target / scale;
    RTC_AlarmTypeDef alarm = {0};
    alarm.Alarm = RTC_ALARM_A;
    alarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
    alarm.AlarmDateWeekDay = 1U;
    alarm.AlarmTime.Hours = sec / 3600U;
    alarm.AlarmTime.Minutes = sec / 60U % 60U;
    alarm.AlarmTime.Seconds = sec % 60U;
    alarm.AlarmTime.SubSeconds = scale - 1U - target % scale;
    alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
    return HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN) == HAL_OK;
}

static uint32_t elapsed_since(uint32_t start, uint32_t scale)
{
    uint32_t now = rtc_position(scale);
    return now >= start ? now - start : (86400U * scale - start) + now;
}

static void stop_until_wake(void)
{
    uint32_t remaining = Countdown_RemainingMs();
    bool timed = remaining != UINT32_MAX;
    if (remaining == 0U || key_wake || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET)
        return;

    uint32_t scale = hrtc.Init.SynchPrediv + 1U;
    uint32_t start = 0U, duration = 0U;
    if (timed) {
        start = rtc_position(scale);
        duration = ((uint64_t)remaining * scale + 999U) / 1000U;
        uint32_t target = (start + duration) % (86400U * scale);
        if (!arm_alarm(target, scale)) {
            HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
            HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
            return; /* 保持软件定时器运行，避免无法按时唤醒。 */
        }
    }

    vTaskSuspendAll();
    uint32_t systick = SysTick->CTRL;
    __disable_irq();
    SysTick->CTRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    HAL_SuspendTick();
    /* PRIMASK 下检查再 WFI，按键/闹钟在两者之间到来也能退出 STOP。 */
    while (!key_wake && !alarm_wake && (!timed || elapsed_since(start, scale) < duration)) {
        __DSB();
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        __enable_irq();
        __ISB();
        __disable_irq();
    }
    __enable_irq();
    HAL_ResumeTick();
    SystemClock_Config(); /* STOP 唤醒后 SYSCLK 为 HSI，恢复 PLL 和 TIM11。 */
    SysTick->VAL = 0U;
    SysTick->CTRL = systick;
    xTaskResumeAll();

    if (timed) {
        HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
        HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
        __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
        HAL_StatusTypeDef synced = HAL_RTC_WaitForSynchro(&hrtc);
        __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
        uint32_t left = remaining;
        if (alarm_wake) left = 0U;
        else if (synced == HAL_OK) {
            uint32_t elapsed = elapsed_since(start, scale);
            left = elapsed >= duration ? 0U : ((duration - elapsed) * 1000U + scale - 1U) / scale;
        }
        Countdown_RestoreMs(left);
    }
}

void Watch_Power_Sleep(void)
{
    key_wake = false;
    alarm_wake = false;
    preparing_sleep = true;
    Stopwatch_PauseForSleep();
    while (samples_in_progress) osDelay(1U);

    /* 等最后一笔 UART 和各传感器整帧采集结束，避免中途切断总线。 */
    osMutexAcquire(UartMutexHandle, osWaitForever);
    bool bluetooth_on = BT_IsEnabled();
    HAL_UART_DeInit(&huart1); /* PA9/PA10 高阻，避免模块断电后经 TX 反灌。 */
    BT_SetEnabled(0U);
    EM7028_Sleep();
    MPU6050_Sleep();
    LSM303DLHC_Sleep();
    SPL06_001_Sleep(); /* AHT21 按次触发，完成后自行待机。 */
    CST816_Sleep();
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
    LCD_ST7789_SleepIn();
    LCD_CS_Set();
    HAL_SPI_DeInit(&hspi1);
    /* EEPROM 两根线外接上拉，休眠时释放，避免通过上拉持续耗电。 */
    const uint32_t eeprom_mask = GPIO_MODER_MODER11 | GPIO_MODER_MODER12;
    uint32_t eeprom_mode = GPIOA->MODER & eeprom_mask;
    SET_BIT(GPIOA->MODER, eeprom_mask); /* 模拟输入，高阻 */

    stop_until_wake();

    MODIFY_REG(GPIOA->MODER, eeprom_mask, eeprom_mode);
    MX_SPI1_Init();
    LCD_CS_Clr();
    LCD_ST7789_SleepOut();
    CST816_Init();
    LCD_Open_Light(); /* 比较值仍为 0；显示任务在恢复图像后调回亮度。 */
    MX_USART1_UART_Init();
    BT_SetEnabled(bluetooth_on);
    osMutexRelease(UartMutexHandle);
    preparing_sleep = false;
}
