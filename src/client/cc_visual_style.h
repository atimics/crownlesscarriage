#ifndef CROWNLESS_VISUAL_STYLE_H
#define CROWNLESS_VISUAL_STYLE_H

#include "raylib.h"

typedef struct CcStyleRamp {
    Color shadow;
    Color base;
    Color light;
} CcStyleRamp;

typedef struct CcCharacterPalette {
    Color skin;
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
    Color teal;
    Color gold;
    Color danger;
    Color violet;
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
    .panel_hover = {24, 38, 43, 248},
    .bar_track = {34, 45, 48, 255},
    .ink = {226, 216, 193, 255},
    .muted = {145, 137, 122, 255},
    .teal = {87, 165, 153, 255},
    .gold = {207, 157, 67, 255},
    .danger = {174, 68, 61, 255},
    .violet = {139, 96, 137, 255},
    .earth = {{57, 43, 36, 255}, {99, 72, 50, 255},
              {146, 103, 65, 255}},
    .road = {{64, 60, 53, 255}, {102, 88, 68, 255},
             {142, 116, 79, 255}},
    .wood = {{52, 36, 31, 255}, {91, 61, 43, 255},
             {137, 88, 50, 255}},
    .stone = {{57, 62, 61, 255}, {91, 96, 91, 255},
              {132, 132, 116, 255}},
    .grass = {{28, 55, 43, 255}, {47, 78, 51, 255},
              {81, 102, 58, 255}},
    .foliage = {{32, 67, 55, 255}, {50, 99, 77, 255},
                {78, 129, 93, 255}},
    .crop = {{73, 79, 40, 255}, {120, 112, 55, 255},
             {171, 136, 54, 255}},
    .metal = {{45, 49, 50, 255}, {91, 103, 101, 255},
              {145, 151, 137, 255}},
    .parchment = {{52, 48, 37, 255}, {91, 78, 49, 255},
                  {214, 197, 151, 255}},
    .contraband = {{57, 34, 55, 255}, {96, 49, 91, 255},
                   {158, 132, 119, 255}},
    /* Identity colors are contracts shared by the model, portrait, and PFP.
       They are named here instead of being approximated from world ramps. */
    .crownless = {
        {177, 131, 93, 255}, {27, 31, 32, 255},
        {47, 108, 106, 255}, {111, 48, 55, 255},
        {40, 48, 57, 255}, {82, 50, 35, 255},
        {139, 55, 62, 255}, {224, 169, 59, 255},
        {43, 32, 29, 255},
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
#define CC_STYLE_TEAL (CC_VISUAL_PALETTE.teal)
#define CC_STYLE_GOLD (CC_VISUAL_PALETTE.gold)
#define CC_STYLE_DANGER (CC_VISUAL_PALETTE.danger)
#define CC_STYLE_VIOLET (CC_VISUAL_PALETTE.violet)
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
#define CC_STYLE_HERO_SKIN (CC_VISUAL_PALETTE.crownless.skin)
#define CC_STYLE_HERO_HAIR (CC_VISUAL_PALETTE.crownless.hair)
#define CC_STYLE_HERO_UNDERLAYER (CC_VISUAL_PALETTE.crownless.underlayer)
#define CC_STYLE_HERO_OUTER (CC_VISUAL_PALETTE.crownless.outer)
#define CC_STYLE_HERO_TROUSERS (CC_VISUAL_PALETTE.crownless.trousers)
#define CC_STYLE_HERO_LEATHER (CC_VISUAL_PALETTE.crownless.leather)
#define CC_STYLE_HERO_METAL (CC_VISUAL_PALETTE.crownless.metal)
#define CC_STYLE_HERO_ACCENT (CC_VISUAL_PALETTE.crownless.accent)
#define CC_STYLE_HERO_PANEL_INK (CC_VISUAL_PALETTE.crownless.panel_ink)

#endif
