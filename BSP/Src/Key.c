#include "Key.h"

key_event_t key_scan(GPIO_TypeDef *port, uint16_t pin)
{
    static key_state_t state = KEY_IDLE;
    static TickType_t  t_enter = 0;
    static uint8_t     long_fired = 0;          
    TickType_t now = xTaskGetTickCount();
    uint8_t is_low = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET);
    key_event_t evt = KEY_EVT_NONE;

    switch (state)
    {
    case KEY_IDLE:
        if (is_low) { state = KEY_DEBOUNCE_DOWN; t_enter = now; }
        break;

    case KEY_DEBOUNCE_DOWN:
        if ((now - t_enter) >= pdMS_TO_TICKS(KEY_DEBOUNCE_MS))
        {
            if (is_low) { state = KEY_PRESSED; long_fired = 0; }   
            else        { state = KEY_IDLE; }                     
            t_enter = now;
        }
        break;

    case KEY_PRESSED:
        if (!is_low) { state = KEY_DEBOUNCE_UP; t_enter = now; }
        else if (!long_fired && (now - t_enter) >= pdMS_TO_TICKS(KEY_LONG_MS))
        {
            evt = KEY_EVT_LONG;        
            long_fired = 1;            
        }
        break;

    case KEY_DEBOUNCE_UP:
        if ((now - t_enter) >= pdMS_TO_TICKS(KEY_DEBOUNCE_MS))
        {
            if (is_low) { state = KEY_PRESSED; t_enter = now; }    
            else
            {
                state = KEY_IDLE;
                if (!long_fired) evt = KEY_EVT_SHORT;              
            }
        }
        break;
    }
    return evt;
}
