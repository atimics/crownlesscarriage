#include "client/cc_visual_style.h"
#include "client/cc_local3d.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct TestOklabColor {
    double lightness;
    double green_red;
    double blue_yellow;
} TestOklabColor;

static double LinearChannel(unsigned char channel)
{
    double value = (double)channel / 255.0;
    return value <= 0.04045 ? value / 12.92 :
        pow((value + 0.055) / 1.055, 2.4);
}

static double RelativeLuminance(Color color)
{
    return 0.2126 * LinearChannel(color.r) +
           0.7152 * LinearChannel(color.g) +
           0.0722 * LinearChannel(color.b);
}

static double ContrastRatio(Color first, Color second)
{
    double first_luminance = RelativeLuminance(first);
    double second_luminance = RelativeLuminance(second);
    double light = fmax(first_luminance, second_luminance);
    double dark = fmin(first_luminance, second_luminance);
    return (light + 0.05) / (dark + 0.05);
}

static TestOklabColor ToOklab(Color color)
{
    double red = LinearChannel(color.r);
    double green = LinearChannel(color.g);
    double blue = LinearChannel(color.b);
    double long_wave = cbrt(0.4122214708 * red + 0.5363325363 * green +
                            0.0514459929 * blue);
    double medium_wave = cbrt(0.2119034982 * red + 0.6806995451 * green +
                              0.1073969566 * blue);
    double short_wave = cbrt(0.0883024619 * red + 0.2817188376 * green +
                             0.6299787005 * blue);
    return (TestOklabColor){
        0.2104542553 * long_wave + 0.7936177850 * medium_wave -
            0.0040720468 * short_wave,
        1.9779984951 * long_wave - 2.4285922050 * medium_wave +
            0.4505937099 * short_wave,
        0.0259040371 * long_wave + 0.7827717662 * medium_wave -
            0.8086757660 * short_wave,
    };
}

static bool RequireContrast(const char *name, Color foreground,
                            Color background, double minimum)
{
    double contrast = ContrastRatio(foreground, background);
    if (contrast >= minimum) return true;
    (void)fprintf(stderr, "%s contrast %.2f is below %.2f\n",
                  name, contrast, minimum);
    return false;
}

static bool RequireLightnessGap(const char *name, Color first, Color second,
                                double minimum)
{
    double gap = fabs(ToOklab(first).lightness - ToOklab(second).lightness);
    if (gap >= minimum) return true;
    (void)fprintf(stderr, "%s lightness gap %.3f is below %.3f\n",
                  name, gap, minimum);
    return false;
}

static bool RequireOrderedRamp(const char *name, CcStyleRamp ramp,
                               double minimum_gap)
{
    double shadow = ToOklab(ramp.shadow).lightness;
    double base = ToOklab(ramp.base).lightness;
    double light = ToOklab(ramp.light).lightness;
    if (base - shadow >= minimum_gap &&
        light - base >= minimum_gap) return true;
    (void)fprintf(stderr,
                  "%s ramp gaps %.3f and %.3f are below %.3f\n",
                  name, base - shadow, light - base, minimum_gap);
    return false;
}

static bool SameRgb(Color color, int red, int green, int blue)
{
    return color.r == red && color.g == green && color.b == blue;
}

int main(void)
{
    bool passed = true;
    passed &= RequireContrast("ink", CC_STYLE_INK, CC_STYLE_PANEL, 4.5);
    passed &= RequireContrast("muted", CC_STYLE_MUTED, CC_STYLE_PANEL, 4.5);
    passed &= RequireContrast("teal", CC_STYLE_TEAL, CC_STYLE_PANEL, 4.5);
    passed &= RequireContrast("gold", CC_STYLE_GOLD, CC_STYLE_PANEL, 4.5);
    passed &= RequireContrast("danger", CC_STYLE_DANGER, CC_STYLE_PANEL, 4.5);
    passed &= RequireContrast("violet", CC_STYLE_VIOLET, CC_STYLE_PANEL, 4.5);

    passed &= RequireLightnessGap("road and earth", CC_STYLE_ROAD,
                                  CC_STYLE_EARTH, 0.08);
    passed &= RequireLightnessGap("earth and wood", CC_STYLE_EARTH,
                                  CC_STYLE_WOOD, 0.06);
    passed &= RequireLightnessGap("metal and stone", CC_STYLE_METAL,
                                  CC_STYLE_STONE, 0.06);
    passed &= RequireLightnessGap("foliage and grass", CC_STYLE_FOLIAGE,
                                  CC_STYLE_GRASS, 0.06);
    passed &= RequireLightnessGap("crop and grass", CC_STYLE_CROP,
                                  CC_STYLE_GRASS, 0.14);

    passed &= RequireOrderedRamp("teal", CC_VISUAL_PALETTE.teal, 0.07);
    passed &= RequireOrderedRamp("gold", CC_VISUAL_PALETTE.gold, 0.07);
    passed &= RequireOrderedRamp("danger", CC_VISUAL_PALETTE.danger, 0.07);
    passed &= RequireOrderedRamp("violet", CC_VISUAL_PALETTE.violet, 0.07);
    passed &= RequireOrderedRamp("earth", CC_VISUAL_PALETTE.earth, 0.07);
    passed &= RequireOrderedRamp("road", CC_VISUAL_PALETTE.road, 0.07);
    passed &= RequireOrderedRamp("wood", CC_VISUAL_PALETTE.wood, 0.07);
    passed &= RequireOrderedRamp("stone", CC_VISUAL_PALETTE.stone, 0.07);
    passed &= RequireOrderedRamp("grass", CC_VISUAL_PALETTE.grass, 0.07);
    passed &= RequireOrderedRamp("foliage", CC_VISUAL_PALETTE.foliage,
                                 0.07);
    passed &= RequireOrderedRamp("crop", CC_VISUAL_PALETTE.crop, 0.07);
    passed &= RequireOrderedRamp("metal", CC_VISUAL_PALETTE.metal, 0.07);
    passed &= RequireOrderedRamp("parchment", CC_VISUAL_PALETTE.parchment,
                                 0.07);
    passed &= RequireOrderedRamp("contraband",
                                 CC_VISUAL_PALETTE.contraband, 0.07);
    passed &= RequireOrderedRamp("people skin",
                                 CC_VISUAL_PALETTE.people_skin, 0.07);

    /* Lock the art direction anchors separately from the hero contract.
       Scene lighting may vary later, but the base world must remain the
       Blackthorn & Brass family. */
    if (!SameRgb(CC_VISUAL_PALETTE.cool_ink, 17, 16, 25) ||
        !SameRgb(CC_STYLE_STONE, 85, 74, 97) ||
        !SameRgb(CC_STYLE_EARTH, 112, 72, 56) ||
        !SameRgb(CC_STYLE_ROAD, 173, 143, 80) ||
        !SameRgb(CC_STYLE_FOLIAGE, 37, 91, 70) ||
        !SameRgb(CC_STYLE_TEAL, 98, 180, 168)) {
        (void)fprintf(stderr, "Blackthorn & Brass anchors changed\n");
        passed = false;
    }

    if (!SameRgb(CC_STYLE_HERO_SKIN, 177, 131, 93) ||
        !SameRgb(CC_STYLE_HERO_HAIR, 27, 31, 32) ||
        !SameRgb(CC_STYLE_HERO_UNDERLAYER, 47, 108, 106) ||
        !SameRgb(CC_STYLE_HERO_OUTER, 111, 48, 55) ||
        !SameRgb(CC_STYLE_HERO_ACCENT, 224, 169, 59)) {
        (void)fprintf(stderr, "Crownless identity colors changed\n");
        passed = false;
    }
    if (CcLocalAtmosphereForDay(0) != CC_LOCAL_ATMOSPHERE_CLEAR_DAY ||
        CcLocalAtmosphereForDay(2) !=
            CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST ||
        CcLocalAtmosphereForDay(4) != CC_LOCAL_ATMOSPHERE_AMBER_DUSK ||
        CcLocalAtmosphereForDay(7) !=
            CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT ||
        CcLocalAtmosphereForDay(8) != CC_LOCAL_ATMOSPHERE_CLEAR_DAY) {
        (void)fprintf(stderr, "authored atmosphere sequence changed\n");
        passed = false;
    }
    if (!passed) return 1;
    (void)printf("visual palette contrast and hierarchy passed\n");
    return 0;
}
