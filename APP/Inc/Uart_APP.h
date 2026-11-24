#ifndef __UART_APP_H
#define __UART_APP_H

#include <stdint.h>

#define UART_TX_ITEM_SIZE     128U
#define BT_SWITCH_CHANGED     (1U << 0)
#define BT_TEST_PERIOD_MS     500U

void BT_SetEnabled(uint8_t enabled);
uint8_t BT_IsEnabled(void);
void Uart_printf(const char *fmt,...);

#endif
