#ifndef __LED_H
#define __LED_H

#include "main.h"
#include "gpio.h"

void LED_ON(void);
void LED_OFF(void);
void LED_Tog(void);

typedef struct{
    GPIO_TypeDef * LED_GPIO;
    uint16_t LED_GPIO_PIN;
}LED_Set;

#endif
