#ifndef __SETTIME_H
#define __SETTIME_H

#include "rtc.h"
#include "lvgl.h"

extern RTC_TimeTypeDef Time;
extern RTC_DateTypeDef Date;

HAL_StatusTypeDef SetTime_Read(void);
void SetTime_Open(lv_event_t *event);

#endif
