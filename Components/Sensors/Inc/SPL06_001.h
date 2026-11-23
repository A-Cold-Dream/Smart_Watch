#ifndef SPL06_001_H
#define SPL06_001_H

#include <stdint.h>

typedef struct
{
    int32_t pressure_pa;
    uint8_t valid;
} SPL06_001_Sample_t;

extern volatile SPL06_001_Sample_t SPL06_001_Sample;
void SPL06_001_Read(void);

#endif
