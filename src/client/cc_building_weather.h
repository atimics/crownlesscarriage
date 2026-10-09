#ifndef CROWNLESS_BUILDING_WEATHER_H
#define CROWNLESS_BUILDING_WEATHER_H

#include "sim/cc_calendar.h"

#include <stdint.h>

typedef struct CcBuildingWeather {
    float snow_cover;
    float wetness;
} CcBuildingWeather;

static inline float CcBuildingWeatherUnit(float value)
{
    if (!(value > 0.0f)) return 0.0f;
    return value < 1.0f ? value : 1.0f;
}

static inline int32_t CcBuildingWeatherPhase(int64_t day, int32_t span)
{
    int32_t phase = (int32_t)(day % span);
    return phase < 0 ? phase + span : phase;
}

static inline int CcBuildingWeatherPrecipitation(int64_t day)
{
    int32_t beat = CcBuildingWeatherPhase(day, 8);
    return beat == 2 || beat == 6;
}

/* Resolve the same weather for every house from the campaign calendar.
   Thirty-two recent days cover accumulation and the short spring thaw.
   Light presets and draw order have no bearing on the result. */
static inline CcBuildingWeather CcBuildingWeatherForDay(int32_t absolute_day)
{
    const int32_t winter_start = CC_SOLAR_DAYS * 3 / 4;
    float cover = 0.0f;
    float thaw = 0.0f;
    float current_cold = 0.0f;
    for (int32_t ago = 31; ago >= 0; --ago) {
        int64_t day = (int64_t)absolute_day - ago;
        int32_t phase = CcBuildingWeatherPhase(day, CC_SOLAR_DAYS);
        float cold = 0.0f;
        float melt = 0.22f;
        if (phase >= winter_start) {
            float onset = CcBuildingWeatherUnit(
                (float)(phase - winter_start + 1) / 14.0f);
            float ending = CcBuildingWeatherUnit(
                (float)(CC_SOLAR_DAYS - phase) / 28.0f);
            cold = onset < ending ? onset : ending;
            if (CcBuildingWeatherPrecipitation(day)) {
                cover += (0.18f + 0.36f * cold) * onset;
            }
            melt = 0.015f + 0.04f * (1.0f - cold);
        } else if (phase < 12) {
            melt = 0.12f + (float)phase * 0.018f;
        }
        float before_melt = CcBuildingWeatherUnit(cover);
        cover = CcBuildingWeatherUnit(before_melt - melt);
        if (ago == 0) {
            current_cold = phase >= winter_start ? 1.0f : 0.0f;
            thaw = (before_melt - cover) * 2.0f;
        }
    }
    float rain = CcBuildingWeatherPrecipitation(absolute_day) ? 0.86f :
        CcBuildingWeatherPrecipitation((int64_t)absolute_day - 1) ? 0.28f : 0.0f;
    return (CcBuildingWeather){
        .snow_cover = cover,
        .wetness = CcBuildingWeatherUnit(rain * (1.0f - current_cold) + thaw),
    };
}

/* Inputs are fractions: slope runs from flat to steep; exposure from
   sheltered to open; warmth from unheated to an active household. */
static inline float CcBuildingRoofSnow(CcBuildingWeather weather,
                                      float roof_slope, float exposure,
                                      float warmth)
{
    return CcBuildingWeatherUnit(weather.snow_cover) *
        (1.0f - 0.55f * CcBuildingWeatherUnit(roof_slope)) *
        CcBuildingWeatherUnit(exposure) *
        (1.0f - 0.20f * CcBuildingWeatherUnit(warmth));
}

#endif
