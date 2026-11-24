/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "AHT21.h"
#include "MPU6050.h"
#include "LSM303DLHC.h"
#include "EM7028.h"
#include "SPL06_001.h"
#include "lvgl.h"
#include "usart.h"
#include "watch_display.h"
#include "Led.h"
#include "Key.h"
#include "iic_hal.h"
#include "Uart_APP.h"
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DISPLAY_KEY1_ACTIVITY (1U << 0)

volatile key_event_t key_state = KEY_EVT_NONE;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for LVGL_Tick */
osThreadId_t LVGL_TickHandle;
const osThreadAttr_t LVGL_Tick_attributes = {
  .name = "LVGL_Tick",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for AHT21_task */
osThreadId_t AHT21_taskHandle;
const osThreadAttr_t AHT21_task_attributes = {
  .name = "AHT21_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MPU6050_task */
osThreadId_t MPU6050_taskHandle;
const osThreadAttr_t MPU6050_task_attributes = {
  .name = "MPU6050_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SPL06_001_task */
osThreadId_t SPL06_001_taskHandle;
const osThreadAttr_t SPL06_001_task_attributes = {
  .name = "SPL06_001_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for EM7028_task */
osThreadId_t EM7028_taskHandle;
const osThreadAttr_t EM7028_task_attributes = {
  .name = "EM7028_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LSM303DLHC_task */
osThreadId_t LSM303DLHC_taskHandle;
const osThreadAttr_t LSM303DLHC_task_attributes = {
  .name = "LSM303DLHC_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for BT_task */
osThreadId_t BT_taskHandle;
const osThreadAttr_t BT_task_attributes = {
  .name = "BT_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for Key_task */
osThreadId_t Key_taskHandle;
const osThreadAttr_t Key_task_attributes = {
  .name = "Key_task",
  .stack_size = 64 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal7,
};
/* Definitions for vUartTxTask */
osThreadId_t vUartTxTaskHandle;
const osThreadAttr_t vUartTxTask_attributes = {
  .name = "vUartTxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for BTTxQueue */
osMessageQueueId_t BTTxQueueHandle;
const osMessageQueueAttr_t BTTxQueue_attributes = {
  .name = "BTTxQueue"
};
/* Definitions for ReadDataMutex */
osMutexId_t ReadDataMutexHandle;
const osMutexAttr_t ReadDataMutex_attributes = {
  .name = "ReadDataMutex"
};
/* Definitions for UartMutex */
osMutexId_t UartMutexHandle;
const osMutexAttr_t UartMutex_attributes = {
  .name = "UartMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartLVGL_Tick(void *argument);
void AHT21_starttask(void *argument);
void MPU6050_starttask(void *argument);
void SPL06_starttask(void *argument);
void EM7028_starttask(void *argument);
void LSM303_starttask(void *argument);
void BT_starttask(void *argument);
void Key_starttask(void *argument);
void vUartTxTaskStart(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of ReadDataMutex */
  ReadDataMutexHandle = osMutexNew(&ReadDataMutex_attributes);

  /* creation of UartMutex */
  UartMutexHandle = osMutexNew(&UartMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of BTTxQueue */
  BTTxQueueHandle = osMessageQueueNew (8, 128, &BTTxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LVGL_Tick */
  LVGL_TickHandle = osThreadNew(StartLVGL_Tick, NULL, &LVGL_Tick_attributes);

  /* creation of AHT21_task */
  AHT21_taskHandle = osThreadNew(AHT21_starttask, NULL, &AHT21_task_attributes);

  /* creation of MPU6050_task */
  MPU6050_taskHandle = osThreadNew(MPU6050_starttask, NULL, &MPU6050_task_attributes);

  /* creation of SPL06_001_task */
  SPL06_001_taskHandle = osThreadNew(SPL06_starttask, NULL, &SPL06_001_task_attributes);

  /* creation of EM7028_task */
  EM7028_taskHandle = osThreadNew(EM7028_starttask, NULL, &EM7028_task_attributes);

  /* creation of LSM303DLHC_task */
  LSM303DLHC_taskHandle = osThreadNew(LSM303_starttask, NULL, &LSM303DLHC_task_attributes);

  /* creation of BT_task */
  BT_taskHandle = osThreadNew(BT_starttask, NULL, &BT_task_attributes);

  /* creation of Key_task */
  Key_taskHandle = osThreadNew(Key_starttask, NULL, &Key_task_attributes);

  /* creation of vUartTxTask */
  vUartTxTaskHandle = osThreadNew(vUartTxTaskStart, NULL, &vUartTxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartLVGL_Tick */
/**
  * @brief  Function implementing the LVGL_Tick thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartLVGL_Tick */
void StartLVGL_Tick(void *argument)
{
  /* USER CODE BEGIN StartLVGL_Tick */
  Watch_Display_Init();
  for(;;)
  {
    uint32_t flags = osThreadFlagsWait(DISPLAY_KEY1_ACTIVITY, osFlagsWaitAny, pdMS_TO_TICKS(5U));
    lv_timer_handler();
    Watch_Display_UpdatePower(flags == DISPLAY_KEY1_ACTIVITY);
  }
  /* USER CODE END StartLVGL_Tick */
}

/* USER CODE BEGIN Header_AHT21_starttask */
/**
* @brief Function implementing the AHT21_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AHT21_starttask */
void AHT21_starttask(void *argument)
{
  /* USER CODE BEGIN AHT21_starttask */
  const TickType_t sample_period = pdMS_TO_TICKS(1000U);
  TickType_t last_wake = xTaskGetTickCount();
  uint8_t ready = 0U;

  for(;;)
  {
    if (!ready)
    {
      ready = AHT21_Init();
      if (ready) last_wake = xTaskGetTickCount();
    }
    if (ready) ready = AHT21_Read();
    vTaskDelayUntil(&last_wake, sample_period);
  }
  /* USER CODE END AHT21_starttask */
}

/* USER CODE BEGIN Header_MPU6050_starttask */
/**
* @brief Function implementing the MPU6050_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_MPU6050_starttask */
void MPU6050_starttask(void *argument)
{
  /* USER CODE BEGIN MPU6050_starttask */

  const TickType_t sample_period = pdMS_TO_TICKS(20U);
  TickType_t last_wake = xTaskGetTickCount();

  /* Infinite loop */
  for(;;)
  {
    MPU6050_Read();
    vTaskDelayUntil(&last_wake, sample_period);
  }
  /* USER CODE END MPU6050_starttask */
}

/* USER CODE BEGIN Header_SPL06_starttask */
/**
* @brief Function implementing the SPL06_001_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_SPL06_starttask */
void SPL06_starttask(void *argument)
{
  /* USER CODE BEGIN SPL06_starttask */
  const TickType_t sample_period = pdMS_TO_TICKS(50U);
  TickType_t last_wake = xTaskGetTickCount();
  /* Infinite loop */
  for(;;)
  {
    SPL06_001_Read();
    vTaskDelayUntil(&last_wake, sample_period);
  }
  /* USER CODE END SPL06_starttask */
}

/* USER CODE BEGIN Header_EM7028_starttask */
/**
* @brief Function implementing the EM7028_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_EM7028_starttask */
void EM7028_starttask(void *argument)
{
  /* USER CODE BEGIN EM7028_starttask */
  const TickType_t sample_period = pdMS_TO_TICKS(10U);
  TickType_t last_wake = xTaskGetTickCount();
  /* Infinite loop */
  for(;;)
  {
    EM7028_Read();
    vTaskDelayUntil(&last_wake, sample_period);
  }
  /* USER CODE END EM7028_starttask */
}

/* USER CODE BEGIN Header_LSM303_starttask */
/**
* @brief Function implementing the LSM303DLHC_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LSM303_starttask */
void LSM303_starttask(void *argument)
{
  /* USER CODE BEGIN LSM303_starttask */
  const TickType_t sample_period = pdMS_TO_TICKS(20U);
  TickType_t last_wake = xTaskGetTickCount();
  /* Infinite loop */
  for(;;)
  {
    LSM303DLHC_Read();
    vTaskDelayUntil(&last_wake, sample_period);
  }
  /* USER CODE END LSM303_starttask */
}

/* USER CODE BEGIN Header_BT_starttask */
/**
* @brief Function implementing the BT_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BT_starttask */
void BT_starttask(void *argument)
{
  /* USER CODE BEGIN BT_starttask */
  /* Infinite loop */
  for(;;)
  {

    uint32_t timeout = BT_IsEnabled() ? pdMS_TO_TICKS(BT_TEST_PERIOD_MS) : osWaitForever;
    uint32_t flags = osThreadFlagsWait(BT_SWITCH_CHANGED, osFlagsWaitAny, timeout);
    if (flags == osFlagsErrorTimeout && BT_IsEnabled())
    {
      Uart_printf("A");
    }
  }
  /* USER CODE END BT_starttask */
}

/* USER CODE BEGIN Header_Key_starttask */
/**
* @brief Function implementing the Key_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Key_starttask */
void Key_starttask(void *argument)
{
  /* USER CODE BEGIN Key_starttask */
  /* Infinite loop */
  for(;;)
  {

    key_state = key_scan(GPIOA, GPIO_PIN_5);
    if (key_is_pressed())
        osThreadFlagsSet(LVGL_TickHandle, DISPLAY_KEY1_ACTIVITY);

    osDelay(pdMS_TO_TICKS(10U));
  }
  /* USER CODE END Key_starttask */
}

/* USER CODE BEGIN Header_vUartTxTaskStart */
/**
* @brief Function implementing the vUartTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vUartTxTaskStart */
void vUartTxTaskStart(void *argument)
{
  /* USER CODE BEGIN vUartTxTaskStart */
  char buf[UART_TX_ITEM_SIZE];
  /* Infinite loop */
  for(;;)
  {

    if (osMessageQueueGet(BTTxQueueHandle, buf, NULL, osWaitForever) != osOK) continue;
    if (osMutexAcquire(UartMutexHandle, pdMS_TO_TICKS(30U)) == osOK)
    {
      if (BT_IsEnabled())
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 30U);
      osMutexRelease(UartMutexHandle);
    }
  }
  /* USER CODE END vUartTxTaskStart */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

uint8_t IIC_SensorBusLock(void)
{
  return osMutexAcquire(ReadDataMutexHandle, pdMS_TO_TICKS(15U)) == osOK;
}

void IIC_SensorBusUnlock(void)
{
  osMutexRelease(ReadDataMutexHandle);
}

/* USER CODE END Application */

