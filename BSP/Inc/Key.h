#ifndef __KEY_H
#define __KEY_H

#include "main.h"
#include "gpio.h"
#include "cmsis_os.h"

typedef enum{
	KEY_IDLE,
	KEY_DEBOUNCE_DOWN,
	KEY_PRESSED,
	KEY_DEBOUNCE_UP
}key_state_t;

typedef enum{
	KEY_EVT_NONE,
	KEY_EVT_SHORT,
	KEY_EVT_LONG
}key_event_t;

#define KEY_DEBOUNCE_MS     20
#define KEY_LONG_MS         800

extern volatile key_event_t key_state;

key_event_t key_scan(GPIO_TypeDef *port, uint16_t pin);

#endif
