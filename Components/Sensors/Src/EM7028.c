#include "EM7028.h"
#include "sensor_i2c.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define EM_ADDR      0x24U
#define PRODUCT_ID   0x00U
#define HRS_CFG      0x01U
#define HRS1_CTRL    0x0DU
#define INT_CTRL     0x0EU
#define HRS1_DATA_L  0x28U
#define HRS1_DATA_H  0x29U

volatile EM7028_Sample_t EM7028_Sample;

static uint8_t ReadReg(uint8_t reg, uint8_t *value)
{
    return SensorI2C_ReadByteWithStop(EM_ADDR, reg, value);
}

static uint8_t Configure(void)
{
    uint8_t id, cfg;
    if (!ReadReg(PRODUCT_ID, &id) || id != 0x36U || !ReadReg(HRS_CFG, &cfg))
        return 0U;
    cfg &= 0x77U;  /* Stop both modes while programming, preserve reserved bits. */
    return SensorI2C_Write(EM_ADDR, HRS_CFG, cfg) &&
           SensorI2C_Write(EM_ADDR, INT_CTRL, 0x00U) &&
           SensorI2C_Write(EM_ADDR, HRS1_CTRL, 0x47U) &&  /* HRS1, 16-bit, gain 1 */
           SensorI2C_Write(EM_ADDR, HRS_CFG, cfg | 0x08U);
}

static uint8_t ReadOptical(uint16_t *raw)
{
    uint8_t cfg, high, low, check, attempt;
    if (!ReadReg(HRS_CFG, &cfg) || (cfg & 0x88U) != 0x08U)
        return 0U;
    /* Random byte reads. Retry if conversion changed the high byte mid-read. */
    for (attempt = 0U; attempt < 3U; attempt++)
    {
        if (!ReadReg(HRS1_DATA_H, &high) || !ReadReg(HRS1_DATA_L, &low) ||
            !ReadReg(HRS1_DATA_H, &check))
            return 0U;
        if (high == check)
        {
            *raw = (uint16_t)((uint16_t)high << 8 | low);
            return 1U;
        }
    }
    return 0U;
}

static uint8_t configured;

void EM7028_Sleep(void)
{
    uint8_t cfg;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
    if (ReadReg(HRS_CFG, &cfg)) SensorI2C_Write(EM_ADDR, HRS_CFG, cfg & 0x77U);
    configured = 0U;
    EM7028_Sample.valid = 0U;
}

void EM7028_Read(void)
{
    EM7028_Sample_t sample = {0};
    if (!configured)
        configured = Configure();
    else
    {
        sample.valid = ReadOptical(&sample.optical_raw);
        configured = sample.valid;
    }
    /* GPIOB.15 is LED_EN for the board's two external low-side LED switches. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, configured ? GPIO_PIN_SET : GPIO_PIN_RESET);
    taskENTER_CRITICAL();
    EM7028_Sample = sample;
    taskEXIT_CRITICAL();
}
