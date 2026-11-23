#ifndef EM7028_H
#define EM7028_H

#include <stdint.h>

typedef struct
{
    uint16_t optical_raw;  /* HRS1_DATA0, 16-bit optical ADC count. */
    uint8_t valid;
} EM7028_Sample_t;

extern volatile EM7028_Sample_t EM7028_Sample;
void EM7028_Read(void);

#endif
