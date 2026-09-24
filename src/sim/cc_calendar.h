#ifndef CC_CALENDAR_H
#define CC_CALENDAR_H
#include <stdint.h>
#include <stddef.h>
#define CC_SOLAR_DAYS 364
#define CC_ZODIAC_SIGNS 13
#define CC_SIGN_DAYS 28
#define CC_SCRIVENDAYS_PHASE 196
/* The existing world clock counts from dawn; its last eight hours are night. */
#define CC_CALENDAR_NIGHT_MINUTE (16 * 60)

typedef struct CcCalendarDate {
    int32_t year, day_of_year, season, sign, day_of_sign, year_sign;
} CcCalendarDate;
CcCalendarDate CcCalendar(int32_t absolute_day);
const char *CcZodiacName(int32_t sign);
const char *CcSeasonName(int32_t season);
void CcCalendarFormat(int32_t day, char *text, size_t capacity);
/* Turns around the sky, shared by observation and drawing. */
double CcCalendarWanderer(int32_t day, int32_t minute);
#endif
