#ifndef CROWNLESS_VISUAL_STYLE_H
#define CROWNLESS_VISUAL_STYLE_H

#include "raylib.h"

typedef struct CcStyleRamp {
    Color shadow;
    Color base;
    Color light;
} CcStyleRamp;

typedef struct CcCharacterPalette {
    Color skin_shadow;
    Color skin;
    Color skin_light;
    Color hair;
    Color underlayer;
    Color outer;
    Color trousers;
    Color leather;
    Color metal;
    Color accent;
    Color panel_ink;
} CcCharacterPalette;

typedef struct CcVisualPalette {
    Color background;
    Color panel;
    Color panel_deep;
    Color panel_hover;
    Color bar_track;
    Color ink;
    Color muted;
    CcStyleRamp teal;
    CcStyleRamp gold;
    CcStyleRamp danger;
    CcStyleRamp violet;
    CcStyleRamp earth;
    CcStyleRamp road;
    CcStyleRamp wood;
    CcStyleRamp stone;
    CcStyleRamp grass;
    CcStyleRamp foliage;
    CcStyleRamp crop;
    CcStyleRamp metal;
    CcStyleRamp parchment;
    CcStyleRamp contraband;
    CcStyleRamp people_skin;
    CcCharacterPalette crownless;
} CcVisualPalette;

/* One authored palette for UI and world materials. Each material has three
   hue-aware bands; lighting may shade within a band, but scene code should
   not invent another near-duplicate brown or green. Warm earth and timber
   separate routes from cool grass and foliage at the final pixel scale. */
static const CcVisualPalette CC_VISUAL_PALETTE = {
    .background = {15, 16, 18, 255},
    .panel = {20, 24, 25, 244},
    .panel_deep = {8, 16, 21, 242},
    .panel_hover = {31, 46, 52, 248},
    .bar_track = {38, 51, 54, 255},
    .ink = {226, 216, 193, 255},
    .muted = {145, 137, 122, 255},
    /* Signal ramps own the brightest chroma. Their light bands are used by
       the UI; the darker bands keep lit world markers in the same family. */
    .teal = {{27, 63, 64, 255}, {57, 133, 125, 255},
             {87, 165, 153, 255}},
    .gold = {{93, 69, 32, 255}, {142, 102, 38, 255},
             {207, 157, 67, 255}},
    .danger = {{66, 36, 43, 255}, {139, 55, 62, 255},
               {209, 93, 81, 255}},
    .violet = {{53, 42, 61, 255}, {98, 70, 103, 255},
               {168, 116, 166, 255}},
    /* Material ramps use perceptually spaced lightness bands. Navigation
       surfaces sit above soil, timber sits below it, and reflective metal
       sits above stone so those pairs survive the final art-pixel scale. */
    .earth = {{59, 41, 33, 255}, {102, 73, 48, 255},
              {149, 106, 64, 255}},
    .road = {{69, 63, 53, 255}, {125, 108, 81, 255},
             {173, 144, 104, 255}},
    .wood = {{51, 33, 28, 255}, {84, 52, 34, 255},
             {135, 86, 47, 255}},
    .stone = {{51, 57, 57, 255}, {86, 93, 87, 255},
              {129, 130, 115, 255}},
    .grass = {{21, 53, 40, 255}, {46, 78, 50, 255},
              {82, 105, 58, 255}},
    .foliage = {{28, 67, 54, 255}, {48, 100, 77, 255},
                {79, 134, 96, 255}},
    .crop = {{75, 81, 42, 255}, {130, 121, 58, 255},
             {182, 147, 63, 255}},
    .metal = {{44, 49, 51, 255}, {102, 117, 116, 255},
              {158, 169, 152, 255}},
    .parchment = {{53, 48, 36, 255}, {112, 98, 66, 255},
                  {201, 182, 132, 255}},
    .contraband = {{58, 33, 56, 255}, {112, 61, 106, 255},
                   {175, 125, 158, 255}},
    /* The full cast may vary in complexion, but final shading resolves to a
       small protected skin family rather than drifting into wood or soil. */
    .people_skin = {{86, 53, 44, 255}, {154, 120, 96, 255},
                    {229, 184, 148, 255}},
    /* Identity colors are contracts shared by the model, portrait, and PFP.
       They are named here instead of being approximated from world ramps. */
    .crownless = {
        .skin_shadow = {111, 75, 57, 255},
        .skin = {177, 131, 93, 255},
        .skin_light = {205, 157, 111, 255},
        .hair = {27, 31, 32, 255},
        .underlayer = {47, 108, 106, 255},
        .outer = {111, 48, 55, 255},
        .trousers = {40, 48, 57, 255},
        .leather = {82, 50, 35, 255},
        .metal = {139, 55, 62, 255},
        .accent = {224, 169, 59, 255},
        .panel_ink = {43, 32, 29, 255},
    },
};

/* Stable names keep existing callers small while all values remain owned by
   the palette above. */
#define CC_STYLE_BACKGROUND (CC_VISUAL_PALETTE.background)
#define CC_STYLE_PANEL (CC_VISUAL_PALETTE.panel)
#define CC_STYLE_PANEL_DEEP (CC_VISUAL_PALETTE.panel_deep)
#define CC_STYLE_PANEL_HOVER (CC_VISUAL_PALETTE.panel_hover)
#define CC_STYLE_BAR_TRACK (CC_VISUAL_PALETTE.bar_track)
#define CC_STYLE_INK (CC_VISUAL_PALETTE.ink)
#define CC_STYLE_MUTED (CC_VISUAL_PALETTE.muted)
#define CC_STYLE_TEAL_SHADOW (CC_VISUAL_PALETTE.teal.shadow)
#define CC_STYLE_TEAL_MID (CC_VISUAL_PALETTE.teal.base)
#define CC_STYLE_TEAL (CC_VISUAL_PALETTE.teal.light)
#define CC_STYLE_GOLD_SHADOW (CC_VISUAL_PALETTE.gold.shadow)
#define CC_STYLE_GOLD_MID (CC_VISUAL_PALETTE.gold.base)
#define CC_STYLE_GOLD (CC_VISUAL_PALETTE.gold.light)
#define CC_STYLE_DANGER_SHADOW (CC_VISUAL_PALETTE.danger.shadow)
#define CC_STYLE_DANGER_MID (CC_VISUAL_PALETTE.danger.base)
#define CC_STYLE_DANGER (CC_VISUAL_PALETTE.danger.light)
#define CC_STYLE_VIOLET_SHADOW (CC_VISUAL_PALETTE.violet.shadow)
#define CC_STYLE_VIOLET_MID (CC_VISUAL_PALETTE.violet.base)
#define CC_STYLE_VIOLET (CC_VISUAL_PALETTE.violet.light)
#define CC_STYLE_EARTH_SHADOW (CC_VISUAL_PALETTE.earth.shadow)
#define CC_STYLE_EARTH (CC_VISUAL_PALETTE.earth.base)
#define CC_STYLE_EARTH_LIGHT (CC_VISUAL_PALETTE.earth.light)
#define CC_STYLE_ROAD_SHADOW (CC_VISUAL_PALETTE.road.shadow)
#define CC_STYLE_ROAD (CC_VISUAL_PALETTE.road.base)
#define CC_STYLE_ROAD_LIGHT (CC_VISUAL_PALETTE.road.light)
#define CC_STYLE_WOOD_SHADOW (CC_VISUAL_PALETTE.wood.shadow)
#define CC_STYLE_WOOD (CC_VISUAL_PALETTE.wood.base)
#define CC_STYLE_WOOD_LIGHT (CC_VISUAL_PALETTE.wood.light)
#define CC_STYLE_STONE_SHADOW (CC_VISUAL_PALETTE.stone.shadow)
#define CC_STYLE_STONE (CC_VISUAL_PALETTE.stone.base)
#define CC_STYLE_STONE_LIGHT (CC_VISUAL_PALETTE.stone.light)
#define CC_STYLE_GRASS_SHADOW (CC_VISUAL_PALETTE.grass.shadow)
#define CC_STYLE_GRASS (CC_VISUAL_PALETTE.grass.base)
#define CC_STYLE_GRASS_LIGHT (CC_VISUAL_PALETTE.grass.light)
#define CC_STYLE_FOLIAGE_SHADOW (CC_VISUAL_PALETTE.foliage.shadow)
#define CC_STYLE_FOLIAGE (CC_VISUAL_PALETTE.foliage.base)
#define CC_STYLE_FOLIAGE_LIGHT (CC_VISUAL_PALETTE.foliage.light)
#define CC_STYLE_CROP_SHADOW (CC_VISUAL_PALETTE.crop.shadow)
#define CC_STYLE_CROP (CC_VISUAL_PALETTE.crop.base)
#define CC_STYLE_CROP_LIGHT (CC_VISUAL_PALETTE.crop.light)
#define CC_STYLE_METAL_SHADOW (CC_VISUAL_PALETTE.metal.shadow)
#define CC_STYLE_METAL (CC_VISUAL_PALETTE.metal.base)
#define CC_STYLE_METAL_LIGHT (CC_VISUAL_PALETTE.metal.light)
#define CC_STYLE_PARCHMENT_SHADOW (CC_VISUAL_PALETTE.parchment.shadow)
#define CC_STYLE_PARCHMENT (CC_VISUAL_PALETTE.parchment.base)
#define CC_STYLE_PARCHMENT_LIGHT (CC_VISUAL_PALETTE.parchment.light)
#define CC_STYLE_CONTRABAND_SHADOW (CC_VISUAL_PALETTE.contraband.shadow)
#define CC_STYLE_CONTRABAND (CC_VISUAL_PALETTE.contraband.base)
#define CC_STYLE_CONTRABAND_LIGHT (CC_VISUAL_PALETTE.contraband.light)
#define CC_STYLE_PEOPLE_SKIN_SHADOW (CC_VISUAL_PALETTE.people_skin.shadow)
#define CC_STYLE_PEOPLE_SKIN (CC_VISUAL_PALETTE.people_skin.base)
#define CC_STYLE_PEOPLE_SKIN_LIGHT (CC_VISUAL_PALETTE.people_skin.light)
#define CC_STYLE_HERO_SKIN (CC_VISUAL_PALETTE.crownless.skin)
#define CC_STYLE_HERO_SKIN_SHADOW \
    (CC_VISUAL_PALETTE.crownless.skin_shadow)
#define CC_STYLE_HERO_SKIN_LIGHT (CC_VISUAL_PALETTE.crownless.skin_light)
#define CC_STYLE_HERO_HAIR (CC_VISUAL_PALETTE.crownless.hair)
#define CC_STYLE_HERO_UNDERLAYER (CC_VISUAL_PALETTE.crownless.underlayer)
#define CC_STYLE_HERO_OUTER (CC_VISUAL_PALETTE.crownless.outer)
#define CC_STYLE_HERO_TROUSERS (CC_VISUAL_PALETTE.crownless.trousers)
#define CC_STYLE_HERO_LEATHER (CC_VISUAL_PALETTE.crownless.leather)
#define CC_STYLE_HERO_METAL (CC_VISUAL_PALETTE.crownless.metal)
#define CC_STYLE_HERO_ACCENT (CC_VISUAL_PALETTE.crownless.accent)
#define CC_STYLE_HERO_PANEL_INK (CC_VISUAL_PALETTE.crownless.panel_ink)

#endif
