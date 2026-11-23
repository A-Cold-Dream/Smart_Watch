#include "sensor_i2c.h"
#include "iic_hal.h"

static iic_bus_t bus = {
    .IIC_SDA_PORT = GPIOB, .IIC_SDA_PIN = GPIO_PIN_13,
    .IIC_SCL_PORT = GPIOB, .IIC_SCL_PIN = GPIO_PIN_14,
};

static uint8_t send_byte(uint8_t value)
{
    IICSendByte(&bus, value);
    /* 底层 IICWaitAck 在 NACK 时已发送 STOP。 */
    return IICWaitAck(&bus) == SUCCESS;
}

static uint8_t select_register(uint8_t address, uint8_t reg)
{
    static uint8_t initialized;
    if (!initialized) {
        IICInit(&bus);
        initialized = 1U;
    }
    const uint32_t pins = GPIO_PIN_13 | GPIO_PIN_14;
    if ((GPIOB->IDR & pins) != pins) return 0U;
    IICStart(&bus);
    return send_byte(address << 1) && send_byte(reg);
}

uint8_t SensorI2C_Write(uint8_t address, uint8_t reg, uint8_t value)
{
    if (!IIC_SensorBusLock()) return 0U;
    uint8_t ok = select_register(address, reg) && send_byte(value);
    if (ok) IICStop(&bus);
    IIC_SensorBusUnlock();
    return ok;
}

static uint8_t read_registers(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length,
                              uint8_t stop_before_read)
{
    if (!length || !IIC_SensorBusLock()) return 0U;
    uint8_t ok = select_register(address, reg);
    if (ok) {
        if (stop_before_read) IICStop(&bus);
        IICStart(&bus);
        ok = send_byte((address << 1) | 1U);
    }
    if (ok) {
        for (unsigned i = 0; i < length; ++i) {
            data[i] = IICReceiveByte(&bus);
            if (i + 1U < length) IICSendAck(&bus);
        }
        IICSendNotAck(&bus);
        IICStop(&bus);
    }
    IIC_SensorBusUnlock();
    return ok;
}

uint8_t SensorI2C_Read(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length)
{
    return read_registers(address, reg, data, length, 0U);
}

uint8_t SensorI2C_ReadByteWithStop(uint8_t address, uint8_t reg, uint8_t *value)
{
    return read_registers(address, reg, value, 1U, 1U);
}
