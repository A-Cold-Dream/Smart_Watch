#ifndef WATCH_TIMER_H
#define WATCH_TIMER_H

#include "lvgl.h"

void Stopwatch_Open(lv_event_t *event);
void Countdown_Open(lv_event_t *event);
void Stopwatch_Refresh(void);
void Stopwatch_PauseForSleep(void);
uint32_t Countdown_RemainingMs(void);
void Countdown_RestoreMs(uint32_t remaining);
void Countdown_Expired(void);

#endif
