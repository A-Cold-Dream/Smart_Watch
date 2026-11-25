#include "MPU6050.h"
#include "FreeRTOS.h"
#include "task.h"
#include "iic_hal.h"

#define MPU6050_ADDRESS       0x68U
#define MPU6050_WHO_AM_I      0x75U
#define MPU6050_PWR_MGMT_1    0x6BU
#define MPU6050_PWR_MGMT_2    0x6CU
#define MPU6050_CONFIG        0x1AU
#define MPU6050_GYRO_CONFIG   0x1BU
#define MPU6050_ACCEL_CONFIG  0x1CU
#define MPU6050_ACCEL_XOUT_H  0x3BU

static iic_bus_t MPU6050_Config = {
    .IIC_SDA_PORT = GPIOB,
    .IIC_SCL_PORT = GPIOB,
    .IIC_SDA_PIN = GPIO_PIN_13,
    .IIC_SCL_PIN = GPIO_PIN_14
};

volatile MPU6050_Sample_t MPU6050_Sample = {0};

static uint8_t SendByte(uint8_t value)
{
    IICSendByte(&MPU6050_Config, value);
    return IICWaitAck(&MPU6050_Config) == SUCCESS;
}

static uint8_t SelectRegister(uint8_t reg)
{
    if(HAL_GPIO_ReadPin(MPU6050_Config.IIC_SCL_PORT, MPU6050_Config.IIC_SCL_PIN) == GPIO_PIN_RESET ||
       HAL_GPIO_ReadPin(MPU6050_Config.IIC_SDA_PORT, MPU6050_Config.IIC_SDA_PIN) == GPIO_PIN_RESET)
       return 0U;
    IICStart(&MPU6050_Config);
    return SendByte(MPU6050_ADDRESS << 1) && SendByte(reg);
}

static uint8_t WriteRegister(uint8_t reg,uint8_t value)
{
    if (!IIC_SensorBusLock())
        return 0U;

    uint8_t written = SelectRegister(reg) && SendByte(value);
    if (written)
        IICStop(&MPU6050_Config);
    IIC_SensorBusUnlock();
    return written;
}

static uint8_t ReadRegisters(uint8_t reg, uint8_t *bytes, uint8_t count)
{
    uint8_t index;
    if (!IIC_SensorBusLock())
        return 0U;

    if (!SelectRegister(reg))
    {
        IIC_SensorBusUnlock();
        return 0U;
    }
    IICStart(&MPU6050_Config);
    if (!SendByte((MPU6050_ADDRESS << 1) | 1U))
    {
        IIC_SensorBusUnlock();
        return 0U;
    }
    for (index = 0U; index < count; index++)
    {
        bytes[index] = IICReceiveByte(&MPU6050_Config);
        if (index + 1U < count)
        {
            IICSendAck(&MPU6050_Config);
        }
    }
    IICSendNotAck(&MPU6050_Config);
    IICStop(&MPU6050_Config);
    IIC_SensorBusUnlock();
    return 1U;
}

static uint8_t MPU6050_Init(void)
{
    uint8_t identity;
    if (!ReadRegisters(MPU6050_WHO_AM_I, &identity, 1U) || identity != 0x68U)
    {
        return 0U;
    }
    if (!WriteRegister(MPU6050_PWR_MGMT_1, 0x01U) ||
        !WriteRegister(MPU6050_PWR_MGMT_2, 0x00U) ||
        !WriteRegister(MPU6050_GYRO_CONFIG, 0x00U) ||
        !WriteRegister(MPU6050_ACCEL_CONFIG, 0x00U) ||
        !WriteRegister(MPU6050_CONFIG, 0x03U))
    {
        return 0U;
    }
    vTaskDelay(pdMS_TO_TICKS(100U));
    return 1U;
}

static int16_t DecodeAxis(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint8_t initialized;

void MPU6050_Sleep(void)
{
    WriteRegister(MPU6050_PWR_MGMT_1, 0x41U); /* SLEEP */
    initialized = 0U;
    MPU6050_Sample.valid = 0U;
}

void MPU6050_Read(void)
{
    MPU6050_Sample_t sample = {0};
    uint8_t frame[14];
    uint8_t axis;

    if (!initialized)
    {
        initialized = MPU6050_Init();
    }
    if (initialized && ReadRegisters(MPU6050_ACCEL_XOUT_H, frame, sizeof(frame)))
    {
        for (axis = 0U; axis < 3U; axis++)
        {
            sample.accel[axis] = DecodeAxis(&frame[2U * axis]);
            sample.gyro[axis] = DecodeAxis(&frame[8U + 2U * axis]);
        }
        sample.valid = 1U;
    }
    initialized = sample.valid;

    taskENTER_CRITICAL();
    MPU6050_Sample = sample;
    taskEXIT_CRITICAL();
}
