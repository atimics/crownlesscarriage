#ifndef CROWNLESS_STYLE_PACK_H
#define CROWNLESS_STYLE_PACK_H

/* Style packs are Crownless's answer to Sierra's AGI/SCI split: one engine
 * binary, and the game's *look* -- shader paths, palette, dither strength,
 * render target size, post-process chain -- chosen at runtime by loading a
 * small resource pack from assets/stylepacks/<id>/style.json.
 *
 * This header is included by more than one translation unit (main.c, and
 * the local3d renderer's own unity build via cc_local3d.c), so the active
 * pack and the palette it can override (see cc_visual_style.h) are extern
 * globals with a single definition in cc_style_pack.c -- both translation
 * units must see the same loaded pack.
 *
 * See docs/design/style-packs.md for the full manifest schema, the
 * reasoning behind what is and is not pack-driven yet, and the design for
 * later (live, non-pre-rendered) painterly passes.
 */

#include "client/cc_visual_style.h"

#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_STYLE_SCHEMA_VERSION 1

#define CC_STYLE_ID_MAX 32
#define CC_STYLE_PATH_MAX 256
#define CC_STYLE_MATERIAL_INK_COUNT 9
#define CC_STYLE_MAX_POST_PASSES 4
#define CC_STYLE_PASS_NAME_MAX 32

typedef enum CcStylePassCache {
    CC_STYLE_PASS_CACHE_PER_FRAME = 0,
    /* Declared for the step-4 painterly pass: paint the static layer once
       when the camera settles, keep it (with depth) across frames, and
       only redraw when the camera moves or the shot's lighting/state
       changes. Not implemented by any pass in this PR. */
    CC_STYLE_PASS_CACHE_PER_SHOT,
} CcStylePassCache;

typedef struct CcStylePostPass {
    char name[CC_STYLE_PASS_NAME_MAX];
    char shader_path[CC_STYLE_PATH_MAX]; /* asset-root relative, resolved */
    bool input_scene_color;
    bool input_scene_depth;  /* reserved: no depth texture is wired yet */
    bool input_scene_normal; /* reserved: no normal buffer is wired yet */
    CcStylePassCache cache;
} CcStylePostPass;

typedef struct CcStylePack {
    char id[CC_STYLE_ID_MAX];
    char version[CC_STYLE_ID_MAX];

    /* Shader roles. All eight are required by every manifest; asset_loading
       .inc's LoadVisualStyle() resolves and loads exactly these, replacing
       the CC_*_SHADER macros a compiled-in style used to hard-code. */
    char world_vertex_shader[CC_STYLE_PATH_MAX];
    char world_fragment_shader[CC_STYLE_PATH_MAX];
    char skinned_vertex_shader[CC_STYLE_PATH_MAX];
    char painted_environment_fragment_shader[CC_STYLE_PATH_MAX];
    char tree_foliage_fragment_shader[CC_STYLE_PATH_MAX];
    char hero_fragment_shader[CC_STYLE_PATH_MAX];
    char npc_fragment_shader[CC_STYLE_PATH_MAX];
    char grade_fragment_shader[CC_STYLE_PATH_MAX];

    /* Optional constants; default to classic's compiled-in values. */
    float hero_ink_strength;
    float material_ink[CC_STYLE_MATERIAL_INK_COUNT];
    float dither_strength; /* 0 for classic; sierra_pixel turns this on */

    /* Optional; defaults to the compiled-in classic palette. Covers both
       the "final palette table" the LUT is built from and the UI/hero
       palette (CcVisualPalette.crownless), since they are the same
       struct. */
    CcVisualPalette palette;

    /* Required. Native/desktop only supports a fixed render target today
       (both shipped packs use 630x320), but the size and the upscale
       filter are read from here, not compiled in. */
    int32_t render_target_width;
    int32_t render_target_height;
    int32_t render_target_upscale_filter; /* a raylib TextureFilter value */

    /* Required, at least one entry. Only a pass named "grade" is actually
       executed in this PR (PresentTarget in actor_rendering.inc); any
       further declared passes are validated for shape and logged, but not
       run yet -- see docs/design/style-packs.md "Future passes". */
    CcStylePostPass post_chain[CC_STYLE_MAX_POST_PASSES];
    int32_t post_chain_count;
} CcStylePack;

/* The live pack. Written once by CcStylePackLoad(), before any window or
   GPU state exists; read afterwards from both main.c and the local3d
   renderer. */
extern CcStylePack g_cc_active_style_pack;

/* Parses and validates assets/stylepacks/<requested_id>/style.json and
 * makes it the active pack. This only reads a small JSON file and fills
 * plain C structs -- it touches no GPU or window state, so it is safe to
 * call at the very top of main(), before InitWindow().
 *
 * Validation is strict: an unknown top-level or nested field, a missing
 * shader role, a schema_version this build does not understand, or any
 * other structural problem rejects the whole pack and logs why. On any
 * rejection -- including "classic" itself failing to parse -- this falls
 * back to the compiled-in classic pack (CcVisualPalette's classic default,
 * the original assets/shaders shader files, and a single grade pass), so a
 * broken style pack can never produce a black screen.
 *
 * requested_id may be NULL or empty, which means "classic".
 */
void CcStylePackLoad(const char *requested_id);

/* The id that ended up active after the most recent CcStylePackLoad call
   (may differ from what was requested if loading fell back to classic). */
const char *CcStylePackActiveId(void);

/* True if the requested pack loaded and validated cleanly (as opposed to
   having fallen back to the compiled-in classic pack). Exposed mainly for
   tests. */
bool CcStylePackLoadedCleanly(void);

#endif
