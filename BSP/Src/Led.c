#include "Led.h"

static LED_Set LED_Config = {
    .LED_GPIO = GPIOB,
    .LED_GPIO_PIN = GPIO_PIN_15
};

void LED_ON(void)
{
    HAL_GPIO_WritePin(LED_Config.LED_GPIO, LED_Config.LED_GPIO_PIN, GPIO_PIN_SET);
}

void LED_OFF(void)
{
    HAL_GPIO_WritePin(LED_Config.LED_GPIO, LED_Config.LED_GPIO_PIN, GPIO_PIN_RESET);
}

void LED_Tog(void)
{
    HAL_GPIO_TogglePin(LED_Config.LED_GPIO, LED_Config.LED_GPIO_PIN);
}
