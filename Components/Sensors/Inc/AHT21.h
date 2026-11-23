#ifndef __AHT21_H
#define __AHT21_H

#include <stdint.h>

typedef enum {
    AHT21_BUS_OK,
    AHT21_SCL_LOW,
    AHT21_SDA_LOW,
    AHT21_BOTH_LOW,
    AHT21_ADDRESS_NACK,
    AHT21_READ_ERROR
}AHT21_Status_t;

typedef struct{
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint8_t valid;
    AHT21_Status_t status;
}AHT21_Sample_t;

extern volatile AHT21_Sample_t AHT21_Sample;

uint8_t AHT21_Init(void);
uint8_t AHT21_Read(void);

#endif
