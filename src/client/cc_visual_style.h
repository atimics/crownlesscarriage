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
    Color cool_ink;
    Color warm_ink;
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

/* Blackthorn & Brass: violet-slate shadows, bottle-green land, tarnished
   brass routes, and oxblood character accents. Each material has three
   hand-authored bands so shadows change hue as well as value. UI neutrals
   live beside the world colors, but are not part of the world lookup. */
static const CcVisualPalette CC_VISUAL_PALETTE = {
    .cool_ink = {17, 16, 25, 255},
    .warm_ink = {33, 23, 26, 255},
    .background = {17, 16, 25, 255},
    .panel = {23, 24, 36, 244},
    .panel_deep = {11, 17, 24, 242},
    .panel_hover = {35, 43, 51, 248},
    .bar_track = {46, 48, 58, 255},
    .ink = {226, 216, 193, 255},
    .muted = {164, 154, 137, 255},
    /* Signal ramps own the brightest chroma. Their light bands are used by
       the UI; the darker bands keep lit world markers in the same family. */
    .teal = {{32, 75, 74, 255}, {63, 132, 125, 255},
             {98, 180, 168, 255}},
    .gold = {{93, 67, 31, 255}, {154, 113, 48, 255},
             {216, 173, 83, 255}},
    .danger = {{66, 36, 43, 255}, {139, 55, 62, 255},
               {209, 93, 81, 255}},
    .violet = {{38, 31, 49, 255}, {89, 74, 104, 255},
               {166, 132, 173, 255}},
    /* Material ramps use perceptually spaced lightness bands. Navigation
       surfaces sit above soil, timber sits below it, and reflective metal
       sits above stone so those pairs survive the final art-pixel scale. */
    .earth = {{60, 41, 48, 255}, {112, 72, 56, 255},
              {167, 111, 79, 255}},
    .road = {{81, 72, 63, 255}, {173, 143, 80, 255},
             {208, 182, 110, 255}},
    .wood = {{48, 31, 36, 255}, {88, 53, 42, 255},
             {145, 96, 61, 255}},
    .stone = {{47, 44, 61, 255}, {85, 74, 97, 255},
              {131, 120, 143, 255}},
    .grass = {{23, 45, 46, 255}, {42, 67, 54, 255},
              {72, 102, 70, 255}},
    .foliage = {{28, 57, 51, 255}, {37, 91, 70, 255},
                {67, 131, 94, 255}},
    .crop = {{92, 81, 46, 255}, {141, 120, 59, 255},
             {184, 155, 76, 255}},
    .metal = {{48, 52, 63, 255}, {104, 114, 125, 255},
              {173, 177, 173, 255}},
    .parchment = {{69, 58, 50, 255}, {129, 112, 74, 255},
                  {201, 182, 132, 255}},
    .contraband = {{53, 35, 63, 255}, {107, 69, 111, 255},
                   {172, 124, 172, 255}},
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
