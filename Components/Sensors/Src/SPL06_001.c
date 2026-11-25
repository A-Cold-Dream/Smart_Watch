#include "SPL06_001.h"
#include "sensor_i2c.h"
#include "FreeRTOS.h"
#include "task.h"

#define SPL_ADDR      0x76U  /* Board R24 pulls SDO low. */
#define PRESSURE_MSB  0x00U
#define PRESSURE_CFG  0x06U
#define TEMP_CFG      0x07U
#define MEAS_CFG      0x08U
#define GENERAL_CFG   0x09U
#define PRODUCT_ID    0x0DU
#define COEFF_C00     0x13U

static struct
{
    int32_t c00, c10;
    int16_t c01, c11, c20, c21, c30;
} cal;

volatile SPL06_001_Sample_t SPL06_001_Sample;

/* All inputs are unsigned fields of exactly 'bits' significant bits. */
static int32_t SignedField(uint32_t value, uint8_t bits)
{
    int32_t sign = 1L << (bits - 1U);
    return (int32_t)(value ^ (uint32_t)sign) - sign;
}

static uint32_t Word(const uint8_t *p)
{
    return (uint32_t)p[0] << 8 | p[1];
}

static uint8_t Configure(void)
{
    uint8_t id, status, p[15];
    if (!SensorI2C_Read(SPL_ADDR, PRODUCT_ID, &id, 1U) || (id >> 4) != 1U ||
        !SensorI2C_Read(SPL_ADDR, MEAS_CFG, &status, 1U) || (status & 0xC0U) != 0xC0U ||
        !SensorI2C_Read(SPL_ADDR, COEFF_C00, p, sizeof(p)))
        return 0U;

    cal.c00 = SignedField((Word(p) << 4) | (p[2] >> 4), 20);
    cal.c10 = SignedField(((uint32_t)(p[2] & 15U) << 16) | Word(p + 3), 20);
    cal.c01 = (int16_t)Word(p + 5);
    cal.c11 = (int16_t)Word(p + 7);
    cal.c20 = (int16_t)Word(p + 9);
    cal.c21 = (int16_t)Word(p + 11);
    cal.c30 = (int16_t)Word(p + 13);

    /* 8 Hz each: pressure x2, MEMS temperature x1, no result shift/FIFO. */
    return SensorI2C_Write(SPL_ADDR, MEAS_CFG, 0x00U) &&
           SensorI2C_Write(SPL_ADDR, PRESSURE_CFG, 0x31U) &&
           SensorI2C_Write(SPL_ADDR, TEMP_CFG, 0xB0U) &&
           SensorI2C_Write(SPL_ADDR, GENERAL_CFG, 0x00U) &&
           SensorI2C_Write(SPL_ADDR, MEAS_CFG, 0x07U);
}

static uint8_t ReadPressure(int32_t *pressure_pa)
{
    uint8_t frame[6];
    float p, t, pa;
    if (!SensorI2C_Read(SPL_ADDR, PRESSURE_MSB, frame, sizeof(frame)))
        return 0U;
    p = SignedField((Word(frame) << 8) | frame[2], 24) / 1572864.0f;
    t = SignedField((Word(frame + 3) << 8) | frame[5], 24) / 524288.0f;
    pa = cal.c00 + p * (cal.c10 + p * (cal.c20 + p * cal.c30)) +
         t * cal.c01 + t * p * (cal.c11 + p * cal.c21);
    if (pa < 30000.0f || pa > 110000.0f)  /* specified operating range */
        return 0U;
    *pressure_pa = (int32_t)(pa + 0.5f);
    return 1U;
}

static uint8_t configured;

void SPL06_001_Sleep(void)
{
    SensorI2C_Write(SPL_ADDR, MEAS_CFG, 0x00U); /* Idle */
    configured = 0U;
    SPL06_001_Sample.valid = 0U;
}

void SPL06_001_Read(void)
{
    static TickType_t last_frame;
    SPL06_001_Sample_t sample = {0};
    if (!configured) {
        configured = Configure();
        last_frame = xTaskGetTickCount();
    } else {
        uint8_t status;
        if (SensorI2C_Read(SPL_ADDR, MEAS_CFG, &status, 1U) && (status & 7U) == 7U) {
            if ((status & 0x30U) == 0x30U) {
                sample.valid = ReadPressure(&sample.pressure_pa);
                last_frame = xTaskGetTickCount();
            } else if (xTaskGetTickCount() - last_frame < pdMS_TO_TICKS(500U)) {
                return;  /* 8 Hz 转换尚未完成，保留上一帧。 */
            }
        }
        configured = sample.valid;
    }
    taskENTER_CRITICAL();
    SPL06_001_Sample = sample;
    taskEXIT_CRITICAL();
}
