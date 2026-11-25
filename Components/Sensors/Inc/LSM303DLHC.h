#ifndef LSM303DLHC_H
#define LSM303DLHC_H

#include <stdint.h>

typedef struct
{
    int16_t accel[3];  /* X/Y/Z, 12-bit counts at +/-2 g (nominally 1 mg/count). */
    int16_t mag[3];    /* X/Y/Z, raw counts. */
    uint8_t accel_valid;
    uint8_t mag_valid;
} LSM303DLHC_Sample_t;

extern volatile LSM303DLHC_Sample_t LSM303DLHC_Sample;
void LSM303DLHC_Read(void);
void LSM303DLHC_Sleep(void);

#endif
