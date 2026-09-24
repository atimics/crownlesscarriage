#include "sim/cc_calendar.h"
#include <stdio.h>

static int32_t Mod(int32_t value, int32_t span)
{
    int32_t result = value % span;
    return result < 0 ? result + span : result;
}
CcCalendarDate CcCalendar(int32_t day)
{
    int32_t phase = Mod(day, CC_SOLAR_DAYS);
    int32_t year = (int32_t)(((int64_t)day - phase) / CC_SOLAR_DAYS);
    return (CcCalendarDate){year, phase + 1, phase / 91,
        phase / CC_SIGN_DAYS, phase % CC_SIGN_DAYS + 1, Mod(year, CC_ZODIAC_SIGNS)};
}
const char *CcZodiacName(int32_t sign)
{
    static const char *names[CC_ZODIAC_SIGNS] = {"Lantern", "Hare", "Hart",
        "Broken Crown", "Hammer", "Cup", "Sheaf", "Quill", "Scales",
        "Gate", "Bell", "Ash Tree", "Wyrm"};
    return sign >= 0 && sign < CC_ZODIAC_SIGNS ? names[sign] : "Unknown sign";
}
const char *CcSeasonName(int32_t season)
{
    static const char *names[4] = {"Spring", "Summer", "Autumn", "Winter"};
    return season >= 0 && season < 4 ? names[season] : "Unknown season";
}
void CcCalendarFormat(int32_t day, char *text, size_t capacity)
{
    CcCalendarDate date = CcCalendar(day);
    if (text != NULL && capacity > 0)
        (void)snprintf(text, capacity, "%d %s | Year of the %s (%d)",
            date.day_of_sign, CcZodiacName(date.sign), CcZodiacName(date.year_sign), date.year);
}
double CcCalendarWanderer(int32_t day, int32_t minute)
{
    const int32_t cycle = CC_SOLAR_DAYS * CC_ZODIAC_SIGNS;
    return ((double)Mod(day, cycle) + (double)minute / 1440.0) / cycle;
}
