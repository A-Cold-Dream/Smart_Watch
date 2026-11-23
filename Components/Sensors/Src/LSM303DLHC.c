#include "LSM303DLHC.h"
#include "sensor_i2c.h"
#include "FreeRTOS.h"
#include "task.h"

#define ACC_ADDR     0x19U
#define ACC_CTRL1    0x20U
#define ACC_CTRL4    0x23U
#define ACC_STATUS   0x27U
#define ACC_X_L      0x28U
#define MAG_ADDR     0x1EU
#define MAG_CONFIG_A 0x00U
#define MAG_CONFIG_B 0x01U
#define MAG_MODE     0x02U
#define MAG_X_H      0x03U
#define MAG_STATUS   0x09U
#define MAG_ID_A     0x0AU

volatile LSM303DLHC_Sample_t LSM303DLHC_Sample;

static uint8_t ReadAccel(int16_t axis[3])
{
    static uint8_t configured;
    uint8_t reg, frame[6], i;
    if (!configured)
    {
        /* BDU, high resolution, +/-2 g. Then start all axes at 50 Hz. */
        configured = SensorI2C_Write(ACC_ADDR, ACC_CTRL4, 0x88U) &&
                     SensorI2C_Write(ACC_ADDR, ACC_CTRL1, 0x47U) &&
                     SensorI2C_Read(ACC_ADDR, ACC_CTRL1, &reg, 1U) && reg == 0x47U;
        return 0U;  /* First conversion will be available on the next task pass. */
    }
    if (!SensorI2C_Read(ACC_ADDR, ACC_STATUS, &reg, 1U) || !(reg & 0x08U) ||
        !SensorI2C_Read(ACC_ADDR, ACC_X_L | 0x80U, frame, sizeof(frame)))
    {
        configured = 0U;
        return 0U;
    }
    for (i = 0U; i < 3U; i++)
    {
        int16_t word = (int16_t)((uint16_t)frame[2U*i + 1U] << 8 | frame[2U*i]);
        axis[i] = word / 16;  /* High-resolution result is left justified. */
    }
    return 1U;
}

static int16_t MagWord(const uint8_t *bytes)
{
    return (int16_t)((uint16_t)bytes[0] << 8 | bytes[1]);
}

static uint8_t ReadMag(int16_t axis[3])
{
    static uint8_t configured;
    uint8_t status, frame[6];
    if (!configured)
    {
        /* The magnetic interface identifies itself as "H43". */
        configured = SensorI2C_Read(MAG_ADDR, MAG_ID_A, frame, 3U) &&
                     frame[0] == 'H' && frame[1] == '4' && frame[2] == '3' &&
                     SensorI2C_Write(MAG_ADDR, MAG_CONFIG_A, 0x10U) && /* 15 Hz */
                     SensorI2C_Write(MAG_ADDR, MAG_CONFIG_B, 0x20U) && /* +/-1.3 gauss */
                     SensorI2C_Write(MAG_ADDR, MAG_MODE, 0x00U);       /* continuous */
        return 0U;
    }
    if (!SensorI2C_Read(MAG_ADDR, MAG_STATUS, &status, 1U) || !(status & 1U) ||
        !SensorI2C_Read(MAG_ADDR, MAG_X_H, frame, sizeof(frame)))
    {
        configured = 0U;
        return 0U;
    }
    /* Magnetometer registers are big-endian and ordered X, Z, Y. */
    axis[0] = MagWord(frame);
    axis[1] = MagWord(frame + 4);
    axis[2] = MagWord(frame + 2);
    return 1U;
}

void LSM303DLHC_Read(void)
{
    LSM303DLHC_Sample_t sample = {0};
    sample.accel_valid = ReadAccel(sample.accel);
    sample.mag_valid = ReadMag(sample.mag);
    taskENTER_CRITICAL();
    LSM303DLHC_Sample = sample;
    taskEXIT_CRITICAL();
}
