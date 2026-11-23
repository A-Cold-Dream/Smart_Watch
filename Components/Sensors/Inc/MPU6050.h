#ifndef __MPU6050_H
#define __MPU6050_H

#include <stdint.h>

typedef struct {
    int16_t accel[3];
    int16_t gyro[3];
    uint8_t valid;
}MPU6050_Sample_t;

extern volatile MPU6050_Sample_t MPU6050_Sample;

void MPU6050_Read(void);

#endif
