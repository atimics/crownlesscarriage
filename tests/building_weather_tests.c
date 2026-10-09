#include "client/cc_building_weather.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    CcBuildingWeather rain = CcBuildingWeatherForDay(98);
    CcBuildingWeather damp = CcBuildingWeatherForDay(99);
    CcBuildingWeather dry = CcBuildingWeatherForDay(100);
    CHECK(rain.snow_cover == 0.0f && rain.wetness > 0.8f);
    CHECK(damp.wetness > 0.2f && damp.wetness < rain.wetness);
    CHECK(dry.wetness == 0.0f && dry.snow_cover == 0.0f);

    CcBuildingWeather autumn = CcBuildingWeatherForDay(272);
    CcBuildingWeather early_winter = CcBuildingWeatherForDay(278);
    CcBuildingWeather winter = CcBuildingWeatherForDay(306);
    CcBuildingWeather after_snow = CcBuildingWeatherForDay(307);
    CHECK(autumn.snow_cover == 0.0f);
    CHECK(early_winter.snow_cover > 0.0f);
    CHECK(early_winter.snow_cover < winter.snow_cover);
    CHECK(winter.snow_cover > 0.5f && winter.wetness < rain.wetness);
    CHECK(after_snow.snow_cover > 0.5f);
    CHECK(after_snow.snow_cover <= winter.snow_cover);

    CcBuildingWeather end_winter = CcBuildingWeatherForDay(363);
    CcBuildingWeather spring = CcBuildingWeatherForDay(364);
    CHECK(end_winter.snow_cover > spring.snow_cover);
    CHECK(spring.snow_cover > 0.0f && spring.wetness > 0.0f);
    float previous = spring.snow_cover;
    for (int32_t day = 365; day < 376; ++day) {
        CcBuildingWeather weather = CcBuildingWeatherForDay(day);
        CHECK(weather.snow_cover <= previous);
        previous = weather.snow_cover;
    }
    CHECK(previous == 0.0f);

    /* A revisit gets the same weather after queries for other dates.
       The day-only API serves day, dusk, night, and omen views. */
    CcBuildingWeather same_day = CcBuildingWeatherForDay(306);
    CHECK(same_day.snow_cover == winter.snow_cover);
    CHECK(same_day.wetness == winter.wetness);

    float open_roof = CcBuildingRoofSnow(winter, 0.0f, 1.0f, 0.0f);
    CHECK(open_roof == winter.snow_cover);
    CHECK(CcBuildingRoofSnow(winter, 1.0f, 1.0f, 0.0f) < open_roof);
    CHECK(CcBuildingRoofSnow(winter, 0.0f, 0.5f, 0.0f) < open_roof);
    CHECK(CcBuildingRoofSnow(winter, 0.0f, 1.0f, 1.0f) < open_roof);
    CHECK(CcBuildingRoofSnow(winter, 0.0f, 0.0f, 0.0f) == 0.0f);
    CHECK(CcBuildingRoofSnow(winter, -1.0f, 3.0f, -1.0f) == open_roof);

    const int32_t edge_days[] = {INT_MIN, INT_MIN + 1, -365, -1, 0, INT_MAX};
    for (int32_t i = 0; i < (int32_t)(sizeof(edge_days) / sizeof(edge_days[0])); ++i) {
        CcBuildingWeather edge = CcBuildingWeatherForDay(edge_days[i]);
        CHECK(isfinite(edge.snow_cover) && edge.snow_cover >= 0.0f && edge.snow_cover <= 1.0f);
        CHECK(isfinite(edge.wetness) && edge.wetness >= 0.0f && edge.wetness <= 1.0f);
    }
    for (int32_t day = -364; day < 728; ++day) {
        CcBuildingWeather weather = CcBuildingWeatherForDay(day);
        CcBuildingWeather repeat = CcBuildingWeatherForDay(day + CC_SOLAR_DAYS * 2);
        CHECK(weather.snow_cover == repeat.snow_cover && weather.wetness == repeat.wetness);
        CHECK(weather.snow_cover >= 0.0f && weather.snow_cover <= 1.0f);
        CHECK(weather.wetness >= 0.0f && weather.wetness <= 1.0f);
    }
    puts("PASS building weather: rain, winter accumulation, spring thaw, roof exposure");
    return 0;
}
