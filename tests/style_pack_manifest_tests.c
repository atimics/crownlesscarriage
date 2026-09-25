/* Manifest loading and validation for the style-pack seam
 * (src/client/cc_style_pack.c). Fixtures live under
 * tests/fixtures/style_pack_manifest/assets/stylepacks/<scenario>/, mirroring
 * the real assets/stylepacks/<id>/ layout so the same path-resolution code
 * runs in both places.
 */

#include "client/cc_local_viewport.h"
#include "client/cc_style_pack.h"
#include "client/cc_visual_style.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if !defined(CC_ASSET_SOURCE_ROOT)
#error "CC_ASSET_SOURCE_ROOT is required to find the test fixtures"
#endif

static bool ChdirOrFail(const char *path)
{
    if (chdir(path) != 0) {
        (void)fprintf(stderr, "could not chdir into %s\n", path);
        return false;
    }
    return true;
}

static bool ExpectCleanLoad(const char *id)
{
    CcStylePackLoad(id);
    if (!CcStylePackLoadedCleanly()) {
        (void)fprintf(stderr, "pack '%s' should have loaded cleanly\n", id);
        return false;
    }
    if (strcmp(CcStylePackActiveId(), id) != 0) {
        (void)fprintf(stderr,
                      "pack '%s' loaded cleanly but active id is '%s'\n", id,
                      CcStylePackActiveId());
        return false;
    }
    return true;
}

static bool ExpectFallbackToClassic(const char *id)
{
    CcStylePackLoad(id);
    if (CcStylePackLoadedCleanly()) {
        (void)fprintf(stderr,
                      "pack '%s' should have failed validation, not loaded "
                      "cleanly\n", id);
        return false;
    }
    if (strcmp(CcStylePackActiveId(), "classic") != 0) {
        (void)fprintf(stderr,
                      "pack '%s' should fall back to classic, got '%s'\n", id,
                      CcStylePackActiveId());
        return false;
    }
    if (g_cc_active_style_pack.post_chain_count < 1 ||
        strcmp(g_cc_active_style_pack.post_chain[0].name, "grade") != 0) {
        (void)fprintf(stderr,
                      "pack '%s' fallback did not install a grade pass\n",
                      id);
        return false;
    }
    return true;
}

int main(void)
{
    bool passed = true;

    if (!ChdirOrFail(CC_ASSET_SOURCE_ROOT "/tests/fixtures/style_pack_manifest")) {
        return 1;
    }

    /* A valid manifest loads and validates cleanly. */
    if (!ExpectCleanLoad("valid_fixture")) passed = false;

    /* Strict validation: reject unknown fields, missing shader roles, and
       an unsupported schema version -- each falling back to the
       compiled-in classic pack, never a black screen. */
    if (!ExpectFallbackToClassic("unknown_field")) passed = false;
    if (!ExpectFallbackToClassic("missing_role")) passed = false;
    if (!ExpectFallbackToClassic("bad_version")) passed = false;

    /* A pack id with no style.json at all falls back the same way. */
    if (!ExpectFallbackToClassic("does_not_exist_at_all")) passed = false;

    /* An empty/NULL request means "classic". */
    CcStylePackLoad(NULL);
    if (!CcStylePackLoadedCleanly() ||
        strcmp(CcStylePackActiveId(), "classic") != 0) {
        (void)fprintf(stderr, "a NULL request should resolve to classic\n");
        passed = false;
    }

    if (!ChdirOrFail(CC_ASSET_SOURCE_ROOT)) return 1;

    /* The real, shipped classic pack must reproduce the compiled-in
       classic look exactly: this is the load-bearing check behind this
       PR's pixel-identity claim. A manifest transcription mistake in
       assets/stylepacks/classic/style.json's palette, ink strength or
       material ink would show up here, in CI, well before anyone captures
       a frame to compare against origin/main. */
    if (!ExpectCleanLoad("classic")) {
        passed = false;
    } else {
        CcVisualPalette classic_default = CC_VISUAL_PALETTE_CLASSIC_INIT;
        if (memcmp(&g_cc_active_style_pack.palette, &classic_default,
                   sizeof(classic_default)) != 0) {
            (void)fprintf(stderr,
                          "classic's manifest palette does not match the "
                          "compiled-in classic palette\n");
            passed = false;
        }
        if (memcmp(&g_cc_active_palette, &classic_default,
                   sizeof(classic_default)) != 0) {
            (void)fprintf(stderr,
                          "the active palette after loading classic does not "
                          "match the compiled-in classic palette\n");
            passed = false;
        }
        if (g_cc_active_style_pack.hero_ink_strength != 0.52f) {
            (void)fprintf(stderr,
                          "classic's manifest hero_ink_strength changed\n");
            passed = false;
        }
        static const float expected_material_ink[CC_STYLE_MATERIAL_INK_COUNT] = {
            0.52f, 0.88f, 0.62f, 0.58f, 0.68f, 0.76f, 0.46f, 0.60f, 0.96f,
        };
        for (int32_t index = 0; index < CC_STYLE_MATERIAL_INK_COUNT; ++index) {
            if (g_cc_active_style_pack.material_ink[index] !=
                expected_material_ink[index]) {
                (void)fprintf(stderr,
                              "classic's manifest material_ink[%d] changed\n",
                              index);
                passed = false;
            }
        }
        if (g_cc_active_style_pack.dither_strength != 0.0f) {
            (void)fprintf(stderr,
                          "classic's dither_strength must stay 0\n");
            passed = false;
        }
        if (CcArtWidth() != 630 || CcArtHeight() != 320) {
            (void)fprintf(stderr,
                          "classic's render target size must stay 630x320\n");
            passed = false;
        }
    }

    /* sierra_pixel must load cleanly, turn dithering on, and otherwise
       keep classic's constants and palette. */
    if (!ExpectCleanLoad("sierra_pixel")) {
        passed = false;
    } else {
        if (g_cc_active_style_pack.dither_strength <= 0.0f) {
            (void)fprintf(stderr,
                          "sierra_pixel should set a positive dither_strength\n");
            passed = false;
        }
        if (CcArtWidth() != 630 || CcArtHeight() != 320) {
            (void)fprintf(stderr,
                          "sierra_pixel's render target size must stay "
                          "630x320 in this PR\n");
            passed = false;
        }
    }

    if (!passed) return 1;
    (void)printf("style pack manifest loading and fallback passed\n");
    return 0;
}
