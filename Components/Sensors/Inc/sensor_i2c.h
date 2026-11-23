#ifndef WATCH_SENSOR_I2C_H
#define WATCH_SENSOR_I2C_H

#include <stdint.h>

uint8_t SensorI2C_Write(uint8_t address, uint8_t reg, uint8_t value);
uint8_t SensorI2C_Read(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length);
uint8_t SensorI2C_ReadByteWithStop(uint8_t address, uint8_t reg, uint8_t *value);

#endif
