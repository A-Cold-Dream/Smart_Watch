#ifndef WATCH_TIMER_H
#define WATCH_TIMER_H

#include "lvgl.h"

void Stopwatch_Open(lv_event_t *event);
void Countdown_Open(lv_event_t *event);
void Stopwatch_Refresh(void);
void Countdown_Expired(void);

#endif
