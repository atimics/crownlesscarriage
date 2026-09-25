/* Style pack loading: a tiny hand-rolled JSON reader plus the manifest
 * validator and fallback described in docs/design/style-packs.md.
 *
 * There is no JSON parsing anywhere else in the C client (the worldpacks
 * language files are only ever read by the Python dialogue tooling), so
 * this file is a small, dependency-free reader restricted to exactly the
 * JSON shapes style.json needs: objects, arrays, strings, numbers and
 * booleans. It is not a general-purpose library and does not try to be
 * one.
 */

#include "client/cc_style_pack.h"

#include "client/cc_local_viewport.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This file builds every error message and every resolved shader path by
 * snprintf-ing one or two CC_STYLE_PATH_MAX (256-byte) strings, plus fixed
 * explanatory text, into another CC_STYLE_PATH_MAX-or-similarly-sized
 * buffer. GCC's -Wformat-truncation reasons from the *declared array size*
 * of each %s argument, not its actual (in every real manifest, much
 * shorter) contents, so it sees "two 256-byte strings into a 256-byte
 * buffer" and flags a truncation that will not occur for any pack id or
 * asset path a person would actually write. Where it could occur (an
 * absurdly long id or path), the result is still safe: a truncated path
 * either fails CcStyleResolveAssetPath's existence check or fails the
 * later strcmp against a known-good value, so the manifest is rejected and
 * this falls back to classic exactly as any other malformed manifest does
 * -- never a wrong shader silently loaded. Clang does not implement this
 * warning, hence the compiler guard. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

/* ---- shared, cross-translation-unit state --------------------------- */

CcVisualPalette g_cc_active_palette = CC_VISUAL_PALETTE_CLASSIC_INIT;
CcStylePack g_cc_active_style_pack = {0};
int32_t cc_active_art_width = CC_LOCAL_ART_WIDTH;
int32_t cc_active_art_height = CC_LOCAL_ART_HEIGHT;
int32_t cc_active_art_upscale_filter = TEXTURE_FILTER_POINT;

static char g_cc_active_style_pack_id[CC_STYLE_ID_MAX] = "classic";
static bool g_cc_active_style_pack_loaded_cleanly = true;

/* The ultimate, hardcoded-in-the-binary fallback. These are the original
   shared shader paths the client always shipped before style packs
   existed; they stay put on disk (see assets/shaders/README or the audit
   notes in docs/design/style-packs.md) specifically so this fallback can
   never itself go missing, even if assets/stylepacks/ is damaged. */
#define CC_STYLE_FALLBACK_WORLD_VERTEX_SHADER "assets/shaders/world_lit.vs"
#define CC_STYLE_FALLBACK_WORLD_FRAGMENT_SHADER "assets/shaders/world_lit.fs"
#define CC_STYLE_FALLBACK_SKINNED_VERTEX_SHADER \
    "assets/shaders/world_lit_skinned.vs"
#define CC_STYLE_FALLBACK_PAINTED_ENVIRONMENT_SHADER \
    "assets/shaders/painted_environment.fs"
#define CC_STYLE_FALLBACK_TREE_FOLIAGE_SHADER \
    "assets/shaders/tree_foliage.fs"
#define CC_STYLE_FALLBACK_HERO_SHADER "assets/shaders/hero_pixel.fs"
#define CC_STYLE_FALLBACK_NPC_SHADER "assets/shaders/npc_indexed.fs"
#define CC_STYLE_FALLBACK_GRADE_SHADER "assets/shaders/style_grade.fs"
#define CC_STYLE_FALLBACK_HERO_INK_STRENGTH 0.52f

static const float CC_STYLE_FALLBACK_MATERIAL_INK[CC_STYLE_MATERIAL_INK_COUNT] = {
    0.52f, 0.88f, 0.62f, 0.58f, 0.68f, 0.76f, 0.46f, 0.60f, 0.96f,
};

static void CcStylePackResetToClassic(CcStylePack *pack)
{
    *pack = (CcStylePack){0};
    (void)snprintf(pack->id, sizeof(pack->id), "classic");
    (void)snprintf(pack->version, sizeof(pack->version), "0.0.0-fallback");
    (void)snprintf(pack->world_vertex_shader,
                   sizeof(pack->world_vertex_shader), "%s",
                   CC_STYLE_FALLBACK_WORLD_VERTEX_SHADER);
    (void)snprintf(pack->world_fragment_shader,
                   sizeof(pack->world_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_WORLD_FRAGMENT_SHADER);
    (void)snprintf(pack->skinned_vertex_shader,
                   sizeof(pack->skinned_vertex_shader), "%s",
                   CC_STYLE_FALLBACK_SKINNED_VERTEX_SHADER);
    (void)snprintf(pack->painted_environment_fragment_shader,
                   sizeof(pack->painted_environment_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_PAINTED_ENVIRONMENT_SHADER);
    (void)snprintf(pack->tree_foliage_fragment_shader,
                   sizeof(pack->tree_foliage_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_TREE_FOLIAGE_SHADER);
    (void)snprintf(pack->hero_fragment_shader,
                   sizeof(pack->hero_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_HERO_SHADER);
    (void)snprintf(pack->npc_fragment_shader,
                   sizeof(pack->npc_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_NPC_SHADER);
    (void)snprintf(pack->grade_fragment_shader,
                   sizeof(pack->grade_fragment_shader), "%s",
                   CC_STYLE_FALLBACK_GRADE_SHADER);
    pack->hero_ink_strength = CC_STYLE_FALLBACK_HERO_INK_STRENGTH;
    for (int32_t index = 0; index < CC_STYLE_MATERIAL_INK_COUNT; ++index) {
        pack->material_ink[index] = CC_STYLE_FALLBACK_MATERIAL_INK[index];
    }
    pack->dither_strength = 0.0f;
    pack->palette = (CcVisualPalette)CC_VISUAL_PALETTE_CLASSIC_INIT;
    pack->render_target_width = CC_LOCAL_ART_WIDTH;
    pack->render_target_height = CC_LOCAL_ART_HEIGHT;
    pack->render_target_upscale_filter = TEXTURE_FILTER_POINT;
    (void)snprintf(pack->post_chain[0].name,
                   sizeof(pack->post_chain[0].name), "grade");
    (void)snprintf(pack->post_chain[0].shader_path,
                   sizeof(pack->post_chain[0].shader_path), "%s",
                   CC_STYLE_FALLBACK_GRADE_SHADER);
    pack->post_chain[0].input_scene_color = true;
    pack->post_chain[0].cache = CC_STYLE_PASS_CACHE_PER_FRAME;
    pack->post_chain_count = 1;
}

/* ---- a small dependency-free JSON reader ----------------------------- */

typedef enum CcJsonType {
    CC_JSON_NULL = 0,
    CC_JSON_BOOL,
    CC_JSON_NUMBER,
    CC_JSON_STRING,
    CC_JSON_ARRAY,
    CC_JSON_OBJECT,
} CcJsonType;

typedef struct CcJsonValue CcJsonValue;

typedef struct CcJsonMember {
    char *key;
    CcJsonValue *value;
} CcJsonMember;

struct CcJsonValue {
    CcJsonType type;
    bool boolean_value;
    double number_value;
    char *string_value;
    CcJsonValue **array_items;
    int32_t array_count;
    CcJsonMember *object_members;
    int32_t object_count;
};

typedef struct CcJsonParser {
    const char *cursor;
    const char *end;
    char error[192];
    bool failed;
} CcJsonParser;

static CcJsonValue *JsonNewValue(CcJsonType type)
{
    CcJsonValue *value = (CcJsonValue *)calloc(1, sizeof(CcJsonValue));
    if (value != NULL) value->type = type;
    return value;
}

static void JsonFree(CcJsonValue *value)
{
    if (value == NULL) return;
    switch (value->type) {
        case CC_JSON_STRING:
            free(value->string_value);
            break;
        case CC_JSON_ARRAY:
            for (int32_t index = 0; index < value->array_count; ++index) {
                JsonFree(value->array_items[index]);
            }
            free(value->array_items);
            break;
        case CC_JSON_OBJECT:
            for (int32_t index = 0; index < value->object_count; ++index) {
                free(value->object_members[index].key);
                JsonFree(value->object_members[index].value);
            }
            free(value->object_members);
            break;
        default:
            break;
    }
    free(value);
}

static void JsonFail(CcJsonParser *parser, const char *message)
{
    if (parser->failed) return;
    parser->failed = true;
    (void)snprintf(parser->error, sizeof(parser->error), "%s", message);
}

static void JsonSkipWhitespace(CcJsonParser *parser)
{
    while (parser->cursor < parser->end) {
        unsigned char character = (unsigned char)*parser->cursor;
        if (character != ' ' && character != '\t' && character != '\n' &&
            character != '\r') {
            break;
        }
        ++parser->cursor;
    }
}

static CcJsonValue *JsonParseValue(CcJsonParser *parser);

static bool JsonExpect(CcJsonParser *parser, char expected)
{
    if (parser->cursor >= parser->end || *parser->cursor != expected) {
        JsonFail(parser, "unexpected character");
        return false;
    }
    ++parser->cursor;
    return true;
}

static char *JsonParseRawString(CcJsonParser *parser)
{
    if (!JsonExpect(parser, '"')) return NULL;
    size_t capacity = 32;
    size_t length = 0;
    char *buffer = (char *)malloc(capacity);
    if (buffer == NULL) {
        JsonFail(parser, "out of memory");
        return NULL;
    }
    while (parser->cursor < parser->end && *parser->cursor != '"') {
        unsigned char character = (unsigned char)*parser->cursor;
        char decoded = (char)character;
        if (character == '\\') {
            ++parser->cursor;
            if (parser->cursor >= parser->end) {
                JsonFail(parser, "string escape cut off");
                free(buffer);
                return NULL;
            }
            char escape = *parser->cursor;
            switch (escape) {
                case '"': decoded = '"'; break;
                case '\\': decoded = '\\'; break;
                case '/': decoded = '/'; break;
                case 'b': decoded = '\b'; break;
                case 'f': decoded = '\f'; break;
                case 'n': decoded = '\n'; break;
                case 'r': decoded = '\r'; break;
                case 't': decoded = '\t'; break;
                case 'u':
                    /* Manifests only ever need ASCII identifiers and asset
                       paths, so a \\uXXXX escape is accepted and folded to
                       '?' rather than fully decoded to UTF-8. */
                    if (parser->end - parser->cursor < 5) {
                        JsonFail(parser, "truncated \\u escape");
                        free(buffer);
                        return NULL;
                    }
                    parser->cursor += 4;
                    decoded = '?';
                    break;
                default:
                    JsonFail(parser, "unknown string escape");
                    free(buffer);
                    return NULL;
            }
        } else if (character < 0x20U) {
            JsonFail(parser, "control character in string");
            free(buffer);
            return NULL;
        }
        if (length + 1U >= capacity) {
            capacity *= 2U;
            char *grown = (char *)realloc(buffer, capacity);
            if (grown == NULL) {
                JsonFail(parser, "out of memory");
                free(buffer);
                return NULL;
            }
            buffer = grown;
        }
        buffer[length] = decoded;
        ++length;
        ++parser->cursor;
    }
    if (!JsonExpect(parser, '"')) {
        free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    return buffer;
}

static CcJsonValue *JsonParseString(CcJsonParser *parser)
{
    char *text = JsonParseRawString(parser);
    if (text == NULL) return NULL;
    CcJsonValue *value = JsonNewValue(CC_JSON_STRING);
    if (value == NULL) {
        free(text);
        JsonFail(parser, "out of memory");
        return NULL;
    }
    value->string_value = text;
    return value;
}

static CcJsonValue *JsonParseNumber(CcJsonParser *parser)
{
    const char *start = parser->cursor;
    if (parser->cursor < parser->end && *parser->cursor == '-') {
        ++parser->cursor;
    }
    while (parser->cursor < parser->end && isdigit((unsigned char)*parser->cursor)) {
        ++parser->cursor;
    }
    if (parser->cursor < parser->end && *parser->cursor == '.') {
        ++parser->cursor;
        while (parser->cursor < parser->end &&
               isdigit((unsigned char)*parser->cursor)) {
            ++parser->cursor;
        }
    }
    if (parser->cursor < parser->end &&
        (*parser->cursor == 'e' || *parser->cursor == 'E')) {
        ++parser->cursor;
        if (parser->cursor < parser->end &&
            (*parser->cursor == '+' || *parser->cursor == '-')) {
            ++parser->cursor;
        }
        while (parser->cursor < parser->end &&
               isdigit((unsigned char)*parser->cursor)) {
            ++parser->cursor;
        }
    }
    if (parser->cursor == start) {
        JsonFail(parser, "invalid number");
        return NULL;
    }
    size_t length = (size_t)(parser->cursor - start);
    char text[64];
    if (length >= sizeof(text)) {
        JsonFail(parser, "number literal too long");
        return NULL;
    }
    memcpy(text, start, length);
    text[length] = '\0';
    CcJsonValue *value = JsonNewValue(CC_JSON_NUMBER);
    if (value == NULL) {
        JsonFail(parser, "out of memory");
        return NULL;
    }
    value->number_value = atof(text);
    return value;
}

static bool JsonMatchLiteral(CcJsonParser *parser, const char *literal)
{
    size_t length = strlen(literal);
    if ((size_t)(parser->end - parser->cursor) < length) return false;
    if (memcmp(parser->cursor, literal, length) != 0) return false;
    parser->cursor += length;
    return true;
}

static CcJsonValue *JsonParseArray(CcJsonParser *parser)
{
    if (!JsonExpect(parser, '[')) return NULL;
    CcJsonValue *value = JsonNewValue(CC_JSON_ARRAY);
    if (value == NULL) {
        JsonFail(parser, "out of memory");
        return NULL;
    }
    JsonSkipWhitespace(parser);
    if (parser->cursor < parser->end && *parser->cursor == ']') {
        ++parser->cursor;
        return value;
    }
    size_t capacity = 4;
    value->array_items = (CcJsonValue **)malloc(capacity * sizeof(CcJsonValue *));
    if (value->array_items == NULL) {
        JsonFail(parser, "out of memory");
        JsonFree(value);
        return NULL;
    }
    for (;;) {
        JsonSkipWhitespace(parser);
        CcJsonValue *item = JsonParseValue(parser);
        if (item == NULL || parser->failed) {
            JsonFree(item);
            JsonFree(value);
            return NULL;
        }
        if ((size_t)value->array_count >= capacity) {
            capacity *= 2U;
            CcJsonValue **grown = (CcJsonValue **)realloc(
                value->array_items, capacity * sizeof(CcJsonValue *));
            if (grown == NULL) {
                JsonFail(parser, "out of memory");
                JsonFree(item);
                JsonFree(value);
                return NULL;
            }
            value->array_items = grown;
        }
        value->array_items[value->array_count] = item;
        ++value->array_count;
        JsonSkipWhitespace(parser);
        if (parser->cursor < parser->end && *parser->cursor == ',') {
            ++parser->cursor;
            continue;
        }
        break;
    }
    JsonSkipWhitespace(parser);
    if (!JsonExpect(parser, ']')) {
        JsonFree(value);
        return NULL;
    }
    return value;
}

static CcJsonValue *JsonParseObject(CcJsonParser *parser)
{
    if (!JsonExpect(parser, '{')) return NULL;
    CcJsonValue *value = JsonNewValue(CC_JSON_OBJECT);
    if (value == NULL) {
        JsonFail(parser, "out of memory");
        return NULL;
    }
    JsonSkipWhitespace(parser);
    if (parser->cursor < parser->end && *parser->cursor == '}') {
        ++parser->cursor;
        return value;
    }
    size_t capacity = 4;
    value->object_members =
        (CcJsonMember *)malloc(capacity * sizeof(CcJsonMember));
    if (value->object_members == NULL) {
        JsonFail(parser, "out of memory");
        JsonFree(value);
        return NULL;
    }
    for (;;) {
        JsonSkipWhitespace(parser);
        char *key = JsonParseRawString(parser);
        if (key == NULL || parser->failed) {
            free(key);
            JsonFree(value);
            return NULL;
        }
        JsonSkipWhitespace(parser);
        if (!JsonExpect(parser, ':')) {
            free(key);
            JsonFree(value);
            return NULL;
        }
        JsonSkipWhitespace(parser);
        CcJsonValue *member_value = JsonParseValue(parser);
        if (member_value == NULL || parser->failed) {
            free(key);
            JsonFree(member_value);
            JsonFree(value);
            return NULL;
        }
        if ((size_t)value->object_count >= capacity) {
            capacity *= 2U;
            CcJsonMember *grown = (CcJsonMember *)realloc(
                value->object_members, capacity * sizeof(CcJsonMember));
            if (grown == NULL) {
                JsonFail(parser, "out of memory");
                free(key);
                JsonFree(member_value);
                JsonFree(value);
                return NULL;
            }
            value->object_members = grown;
        }
        value->object_members[value->object_count].key = key;
        value->object_members[value->object_count].value = member_value;
        ++value->object_count;
        JsonSkipWhitespace(parser);
        if (parser->cursor < parser->end && *parser->cursor == ',') {
            ++parser->cursor;
            continue;
        }
        break;
    }
    JsonSkipWhitespace(parser);
    if (!JsonExpect(parser, '}')) {
        JsonFree(value);
        return NULL;
    }
    return value;
}

static CcJsonValue *JsonParseValue(CcJsonParser *parser)
{
    JsonSkipWhitespace(parser);
    if (parser->cursor >= parser->end) {
        JsonFail(parser, "unexpected end of input");
        return NULL;
    }
    char next = *parser->cursor;
    if (next == '{') return JsonParseObject(parser);
    if (next == '[') return JsonParseArray(parser);
    if (next == '"') return JsonParseString(parser);
    if (next == '-' || isdigit((unsigned char)next)) {
        return JsonParseNumber(parser);
    }
    if (JsonMatchLiteral(parser, "true")) {
        CcJsonValue *value = JsonNewValue(CC_JSON_BOOL);
        if (value != NULL) value->boolean_value = true;
        return value;
    }
    if (JsonMatchLiteral(parser, "false")) {
        CcJsonValue *value = JsonNewValue(CC_JSON_BOOL);
        if (value != NULL) value->boolean_value = false;
        return value;
    }
    if (JsonMatchLiteral(parser, "null")) {
        return JsonNewValue(CC_JSON_NULL);
    }
    JsonFail(parser, "unrecognized value");
    return NULL;
}

static CcJsonValue *JsonParseDocument(const char *text, char *error,
                                      size_t error_capacity)
{
    CcJsonParser parser = {0};
    parser.cursor = text;
    parser.end = text + strlen(text);
    CcJsonValue *root = JsonParseValue(&parser);
    if (root != NULL && !parser.failed) {
        JsonSkipWhitespace(&parser);
        if (parser.cursor != parser.end) {
            JsonFail(&parser, "trailing data after document");
        }
    }
    if (parser.failed) {
        JsonFree(root);
        (void)snprintf(error, error_capacity, "%s", parser.error);
        return NULL;
    }
    return root;
}

/* ---- lookup and typed-access helpers ---------------------------------- */

static const CcJsonValue *JsonObjectGet(const CcJsonValue *object,
                                        const char *key)
{
    if (object == NULL || object->type != CC_JSON_OBJECT) return NULL;
    for (int32_t index = 0; index < object->object_count; ++index) {
        if (strcmp(object->object_members[index].key, key) == 0) {
            return object->object_members[index].value;
        }
    }
    return NULL;
}

/* Rejects a manifest object that has any key outside `allowed`. Returns the
   offending key through *out_unknown_key on failure. */
static bool JsonObjectKeysAllowed(const CcJsonValue *object,
                                  const char *const *allowed,
                                  int32_t allowed_count,
                                  const char **out_unknown_key)
{
    if (object == NULL || object->type != CC_JSON_OBJECT) return true;
    for (int32_t index = 0; index < object->object_count; ++index) {
        const char *key = object->object_members[index].key;
        bool found = false;
        for (int32_t allow_index = 0; allow_index < allowed_count;
             ++allow_index) {
            if (strcmp(key, allowed[allow_index]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (out_unknown_key != NULL) *out_unknown_key = key;
            return false;
        }
    }
    return true;
}

static bool JsonAsDouble(const CcJsonValue *value, double *out)
{
    if (value == NULL || value->type != CC_JSON_NUMBER) return false;
    *out = value->number_value;
    return true;
}

static bool JsonAsFloat(const CcJsonValue *value, float *out)
{
    double number = 0.0;
    if (!JsonAsDouble(value, &number)) return false;
    *out = (float)number;
    return true;
}

static bool JsonAsInt(const CcJsonValue *value, int32_t *out)
{
    double number = 0.0;
    if (!JsonAsDouble(value, &number)) return false;
    *out = (int32_t)lround(number);
    return true;
}

static bool JsonAsChannel(const CcJsonValue *value, unsigned char *out)
{
    int32_t number = 0;
    if (!JsonAsInt(value, &number)) return false;
    if (number < 0 || number > 255) return false;
    *out = (unsigned char)number;
    return true;
}

static bool JsonAsString(const CcJsonValue *value, char *out, size_t capacity)
{
    if (value == NULL || value->type != CC_JSON_STRING) return false;
    size_t length = strlen(value->string_value);
    if (length + 1U > capacity) return false;
    memcpy(out, value->string_value, length + 1U);
    return true;
}

static bool JsonAsStringEquals(const CcJsonValue *value, const char *text)
{
    return value != NULL && value->type == CC_JSON_STRING &&
        strcmp(value->string_value, text) == 0;
}

/* A color is a JSON array of 3 (RGB, alpha defaults to 255) or 4 (RGBA)
   integers in 0..255. */
static bool JsonAsColor(const CcJsonValue *value, Color *out)
{
    if (value == NULL || value->type != CC_JSON_ARRAY) return false;
    if (value->array_count != 3 && value->array_count != 4) return false;
    unsigned char channels[4] = {0, 0, 0, 255};
    for (int32_t index = 0; index < value->array_count; ++index) {
        if (!JsonAsChannel(value->array_items[index], &channels[index])) {
            return false;
        }
    }
    out->r = channels[0];
    out->g = channels[1];
    out->b = channels[2];
    out->a = channels[3];
    return true;
}

static bool JsonAsRamp(const CcJsonValue *value, CcStyleRamp *out)
{
    static const char *const allowed[] = {"shadow", "base", "light"};
    const char *unknown = NULL;
    if (!JsonObjectKeysAllowed(value, allowed, 3, &unknown)) return false;
    return JsonAsColor(JsonObjectGet(value, "shadow"), &out->shadow) &&
        JsonAsColor(JsonObjectGet(value, "base"), &out->base) &&
        JsonAsColor(JsonObjectGet(value, "light"), &out->light);
}

/* ---- manifest -> CcStylePack ------------------------------------------ */

/* asset-root-relative path resolution, mirroring
   local3d/asset_loading.inc's static ResolveAssetPath -- duplicated
   because that helper is private to a different translation unit (the
   local3d renderer's own unity build), and this file needs the same
   search order before any GPU/window state exists. */
static bool CcStyleResolveAssetPath(const char *relative_path, char *resolved,
                                    size_t capacity)
{
    if (relative_path == NULL || resolved == NULL || capacity == 0U) {
        return false;
    }
    if (FileExists(relative_path)) {
        (void)snprintf(resolved, capacity, "%s", relative_path);
        return true;
    }
#if defined(CC_ASSET_SOURCE_ROOT)
    (void)snprintf(resolved, capacity, "%s/%s", CC_ASSET_SOURCE_ROOT,
                   relative_path);
    if (FileExists(resolved)) return true;
#endif
    (void)snprintf(resolved, capacity, "../%s", relative_path);
    if (FileExists(resolved)) return true;
    (void)snprintf(resolved, capacity, "%s/../Resources/%s",
                   GetApplicationDirectory(), relative_path);
    return FileExists(resolved);
}

typedef struct CcStyleShaderRole {
    const char *json_key;
    size_t field_offset;
} CcStyleShaderRole;

#define CC_STYLE_SHADER_ROLE(json_key, field) \
    { (json_key), offsetof(CcStylePack, field) }

static const CcStyleShaderRole CC_STYLE_SHADER_ROLES[] = {
    CC_STYLE_SHADER_ROLE("world_vertex", world_vertex_shader),
    CC_STYLE_SHADER_ROLE("world_fragment", world_fragment_shader),
    CC_STYLE_SHADER_ROLE("skinned_vertex", skinned_vertex_shader),
    CC_STYLE_SHADER_ROLE("painted_environment_fragment",
                         painted_environment_fragment_shader),
    CC_STYLE_SHADER_ROLE("tree_foliage_fragment", tree_foliage_fragment_shader),
    CC_STYLE_SHADER_ROLE("hero_fragment", hero_fragment_shader),
    CC_STYLE_SHADER_ROLE("npc_fragment", npc_fragment_shader),
    CC_STYLE_SHADER_ROLE("grade_fragment", grade_fragment_shader),
};
#define CC_STYLE_SHADER_ROLE_COUNT \
    (int32_t)(sizeof(CC_STYLE_SHADER_ROLES) / sizeof(CC_STYLE_SHADER_ROLES[0]))

/* Loads and validates one manifest, without touching the pack directory of
   any *other* pack. On success fills `pack` completely (every optional
   field defaulted to classic first) and returns true; on failure writes a
   human-readable reason to `error` and returns false, leaving `pack`
   unspecified. */
static bool CcStylePackParseFromDisk(const char *id, CcStylePack *pack,
                                     char *error, size_t error_capacity)
{
    char manifest_relative[CC_STYLE_PATH_MAX];
    (void)snprintf(manifest_relative, sizeof(manifest_relative),
                   "assets/stylepacks/%s/style.json", id);
    char pack_directory[CC_STYLE_PATH_MAX];
    (void)snprintf(pack_directory, sizeof(pack_directory),
                   "assets/stylepacks/%s", id);

    char manifest_path[CC_STYLE_PATH_MAX];
    if (!CcStyleResolveAssetPath(manifest_relative, manifest_path,
                                 sizeof(manifest_path))) {
        (void)snprintf(error, error_capacity,
                       "no style.json found for pack '%s'", id);
        return false;
    }
    char *text = LoadFileText(manifest_path);
    if (text == NULL) {
        (void)snprintf(error, error_capacity,
                       "could not read '%s'", manifest_path);
        return false;
    }
    char json_error[192];
    CcJsonValue *root = JsonParseDocument(text, json_error, sizeof(json_error));
    UnloadFileText(text);
    if (root == NULL) {
        (void)snprintf(error, error_capacity, "%s: %s", manifest_path,
                       json_error);
        return false;
    }
    if (root->type != CC_JSON_OBJECT) {
        (void)snprintf(error, error_capacity,
                       "%s: manifest root must be an object", manifest_path);
        JsonFree(root);
        return false;
    }

    static const char *const top_level_keys[] = {
        "schema_version", "id", "version", "shaders",
        "constants", "palette", "render_target", "post_chain",
    };
    const char *unknown = NULL;
    if (!JsonObjectKeysAllowed(root, top_level_keys,
                              (int32_t)(sizeof(top_level_keys) /
                                       sizeof(top_level_keys[0])),
                              &unknown)) {
        (void)snprintf(error, error_capacity,
                       "%s: unknown top-level field '%s'", manifest_path,
                       unknown);
        JsonFree(root);
        return false;
    }

    int32_t schema_version = 0;
    if (!JsonAsInt(JsonObjectGet(root, "schema_version"), &schema_version) ||
        schema_version != CC_STYLE_SCHEMA_VERSION) {
        (void)snprintf(error, error_capacity,
                       "%s: schema_version must be %d", manifest_path,
                       CC_STYLE_SCHEMA_VERSION);
        JsonFree(root);
        return false;
    }

    CcStylePackResetToClassic(pack);

    char manifest_id[CC_STYLE_ID_MAX];
    if (!JsonAsString(JsonObjectGet(root, "id"), manifest_id,
                      sizeof(manifest_id)) ||
        strcmp(manifest_id, id) != 0) {
        (void)snprintf(error, error_capacity,
                       "%s: \"id\" must be the string \"%s\"", manifest_path,
                       id);
        JsonFree(root);
        return false;
    }
    (void)snprintf(pack->id, sizeof(pack->id), "%s", manifest_id);

    if (!JsonAsString(JsonObjectGet(root, "version"), pack->version,
                      sizeof(pack->version))) {
        (void)snprintf(error, error_capacity,
                       "%s: \"version\" must be a string", manifest_path);
        JsonFree(root);
        return false;
    }

    /* -- shaders (required object, all eight roles required) -- */
    const CcJsonValue *shaders = JsonObjectGet(root, "shaders");
    if (shaders == NULL || shaders->type != CC_JSON_OBJECT) {
        (void)snprintf(error, error_capacity,
                       "%s: \"shaders\" object is required", manifest_path);
        JsonFree(root);
        return false;
    }
    static const char *const shader_keys[] = {
        "world_vertex", "world_fragment", "skinned_vertex",
        "painted_environment_fragment", "tree_foliage_fragment",
        "hero_fragment", "npc_fragment", "grade_fragment",
    };
    if (!JsonObjectKeysAllowed(shaders, shader_keys,
                              (int32_t)(sizeof(shader_keys) /
                                       sizeof(shader_keys[0])),
                              &unknown)) {
        (void)snprintf(error, error_capacity,
                       "%s: unknown field \"shaders.%s\"", manifest_path,
                       unknown);
        JsonFree(root);
        return false;
    }
    for (int32_t role_index = 0; role_index < CC_STYLE_SHADER_ROLE_COUNT;
         ++role_index) {
        const CcStyleShaderRole *role = &CC_STYLE_SHADER_ROLES[role_index];
        char relative_shader[CC_STYLE_PATH_MAX];
        if (!JsonAsString(JsonObjectGet(shaders, role->json_key),
                          relative_shader, sizeof(relative_shader))) {
            (void)snprintf(error, error_capacity,
                           "%s: missing shader role \"shaders.%s\"",
                           manifest_path, role->json_key);
            JsonFree(root);
            return false;
        }
        char *field = (char *)pack + role->field_offset;
        char full_relative[CC_STYLE_PATH_MAX];
        (void)snprintf(full_relative, sizeof(full_relative), "%s/%s",
                       pack_directory, relative_shader);
        char resolved[CC_STYLE_PATH_MAX];
        if (!CcStyleResolveAssetPath(full_relative, resolved,
                                     sizeof(resolved))) {
            (void)snprintf(error, error_capacity,
                           "%s: shader role \"shaders.%s\" points at a "
                           "file that does not exist (%s)",
                           manifest_path, role->json_key, full_relative);
            JsonFree(root);
            return false;
        }
        (void)snprintf(field, CC_STYLE_PATH_MAX, "%s", full_relative);
    }

    /* -- constants (optional; default already set by ResetToClassic) -- */
    const CcJsonValue *constants = JsonObjectGet(root, "constants");
    if (constants != NULL) {
        static const char *const constant_keys[] = {
            "hero_ink_strength", "material_ink", "dither_strength",
        };
        if (!JsonObjectKeysAllowed(constants, constant_keys,
                                  (int32_t)(sizeof(constant_keys) /
                                           sizeof(constant_keys[0])),
                                  &unknown)) {
            (void)snprintf(error, error_capacity,
                           "%s: unknown field \"constants.%s\"",
                           manifest_path, unknown);
            JsonFree(root);
            return false;
        }
        const CcJsonValue *ink_strength =
            JsonObjectGet(constants, "hero_ink_strength");
        if (ink_strength != NULL &&
            !JsonAsFloat(ink_strength, &pack->hero_ink_strength)) {
            (void)snprintf(error, error_capacity,
                           "%s: \"constants.hero_ink_strength\" must be a "
                           "number", manifest_path);
            JsonFree(root);
            return false;
        }
        const CcJsonValue *material_ink =
            JsonObjectGet(constants, "material_ink");
        if (material_ink != NULL) {
            if (material_ink->type != CC_JSON_ARRAY ||
                material_ink->array_count != CC_STYLE_MATERIAL_INK_COUNT) {
                (void)snprintf(error, error_capacity,
                               "%s: \"constants.material_ink\" must have "
                               "%d numbers", manifest_path,
                               CC_STYLE_MATERIAL_INK_COUNT);
                JsonFree(root);
                return false;
            }
            for (int32_t index = 0; index < CC_STYLE_MATERIAL_INK_COUNT;
                 ++index) {
                if (!JsonAsFloat(material_ink->array_items[index],
                                &pack->material_ink[index])) {
                    (void)snprintf(error, error_capacity,
                                   "%s: \"constants.material_ink[%d]\" must "
                                   "be a number", manifest_path, index);
                    JsonFree(root);
                    return false;
                }
            }
        }
        const CcJsonValue *dither = JsonObjectGet(constants, "dither_strength");
        if (dither != NULL && !JsonAsFloat(dither, &pack->dither_strength)) {
            (void)snprintf(error, error_capacity,
                           "%s: \"constants.dither_strength\" must be a "
                           "number", manifest_path);
            JsonFree(root);
            return false;
        }
    }

    /* -- palette (optional; default already set by ResetToClassic) -- */
    const CcJsonValue *palette = JsonObjectGet(root, "palette");
    if (palette != NULL) {
        static const char *const palette_keys[] = {
            "cool_ink", "warm_ink", "background", "panel", "panel_deep",
            "panel_hover", "bar_track", "ink", "muted", "teal", "gold",
            "danger", "violet", "earth", "road", "wood", "stone", "grass",
            "foliage", "crop", "metal", "parchment", "contraband",
            "people_skin", "crownless", "contact_shadow_soft",
            "contact_shadow_strong", "road_dust", "footstep_print",
            "robot_chassis_teal", "robot_chassis_gold", "robot_limb_dark",
            "robot_skin_bronze",
        };
        if (!JsonObjectKeysAllowed(palette, palette_keys,
                                  (int32_t)(sizeof(palette_keys) /
                                           sizeof(palette_keys[0])),
                                  &unknown)) {
            (void)snprintf(error, error_capacity,
                           "%s: unknown field \"palette.%s\"",
                           manifest_path, unknown);
            JsonFree(root);
            return false;
        }
        const char *bad_field = NULL;
        bool ok = true;
        ok = ok && JsonAsColor(JsonObjectGet(palette, "cool_ink"),
                              &pack->palette.cool_ink);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "warm_ink"),
                              &pack->palette.warm_ink);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "background"),
                              &pack->palette.background);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "panel"),
                              &pack->palette.panel);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "panel_deep"),
                              &pack->palette.panel_deep);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "panel_hover"),
                              &pack->palette.panel_hover);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "bar_track"),
                              &pack->palette.bar_track);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "ink"),
                              &pack->palette.ink);
        ok = ok && JsonAsColor(JsonObjectGet(palette, "muted"),
                              &pack->palette.muted);
        if (!ok) bad_field = "background/panel/ink/muted";
        static const struct {
            const char *key;
            size_t offset;
        } ramp_fields[] = {
            {"teal", offsetof(CcVisualPalette, teal)},
            {"gold", offsetof(CcVisualPalette, gold)},
            {"danger", offsetof(CcVisualPalette, danger)},
            {"violet", offsetof(CcVisualPalette, violet)},
            {"earth", offsetof(CcVisualPalette, earth)},
            {"road", offsetof(CcVisualPalette, road)},
            {"wood", offsetof(CcVisualPalette, wood)},
            {"stone", offsetof(CcVisualPalette, stone)},
            {"grass", offsetof(CcVisualPalette, grass)},
            {"foliage", offsetof(CcVisualPalette, foliage)},
            {"crop", offsetof(CcVisualPalette, crop)},
            {"metal", offsetof(CcVisualPalette, metal)},
            {"parchment", offsetof(CcVisualPalette, parchment)},
            {"contraband", offsetof(CcVisualPalette, contraband)},
            {"people_skin", offsetof(CcVisualPalette, people_skin)},
        };
        for (size_t index = 0;
             ok && index < sizeof(ramp_fields) / sizeof(ramp_fields[0]);
             ++index) {
            CcStyleRamp *ramp =
                (CcStyleRamp *)((char *)&pack->palette + ramp_fields[index].offset);
            if (!JsonAsRamp(JsonObjectGet(palette, ramp_fields[index].key),
                           ramp)) {
                ok = false;
                bad_field = ramp_fields[index].key;
            }
        }
        const CcJsonValue *crownless = JsonObjectGet(palette, "crownless");
        if (ok) {
            static const char *const crownless_keys[] = {
                "skin_shadow", "skin", "skin_light", "hair", "underlayer",
                "outer", "trousers", "leather", "metal", "accent",
                "panel_ink",
            };
            if (!JsonObjectKeysAllowed(crownless, crownless_keys,
                                      (int32_t)(sizeof(crownless_keys) /
                                               sizeof(crownless_keys[0])),
                                      &unknown)) {
                ok = false;
                bad_field = "crownless";
            }
        }
        ok = ok &&
            JsonAsColor(JsonObjectGet(crownless, "skin_shadow"),
                       &pack->palette.crownless.skin_shadow) &&
            JsonAsColor(JsonObjectGet(crownless, "skin"),
                       &pack->palette.crownless.skin) &&
            JsonAsColor(JsonObjectGet(crownless, "skin_light"),
                       &pack->palette.crownless.skin_light) &&
            JsonAsColor(JsonObjectGet(crownless, "hair"),
                       &pack->palette.crownless.hair) &&
            JsonAsColor(JsonObjectGet(crownless, "underlayer"),
                       &pack->palette.crownless.underlayer) &&
            JsonAsColor(JsonObjectGet(crownless, "outer"),
                       &pack->palette.crownless.outer) &&
            JsonAsColor(JsonObjectGet(crownless, "trousers"),
                       &pack->palette.crownless.trousers) &&
            JsonAsColor(JsonObjectGet(crownless, "leather"),
                       &pack->palette.crownless.leather) &&
            JsonAsColor(JsonObjectGet(crownless, "metal"),
                       &pack->palette.crownless.metal) &&
            JsonAsColor(JsonObjectGet(crownless, "accent"),
                       &pack->palette.crownless.accent) &&
            JsonAsColor(JsonObjectGet(crownless, "panel_ink"),
                       &pack->palette.crownless.panel_ink);
        if (!ok && bad_field == NULL) bad_field = "crownless";
        ok = ok &&
            JsonAsColor(JsonObjectGet(palette, "contact_shadow_soft"),
                       &pack->palette.contact_shadow_soft) &&
            JsonAsColor(JsonObjectGet(palette, "contact_shadow_strong"),
                       &pack->palette.contact_shadow_strong) &&
            JsonAsColor(JsonObjectGet(palette, "road_dust"),
                       &pack->palette.road_dust) &&
            JsonAsColor(JsonObjectGet(palette, "footstep_print"),
                       &pack->palette.footstep_print) &&
            JsonAsColor(JsonObjectGet(palette, "robot_chassis_teal"),
                       &pack->palette.robot_chassis_teal) &&
            JsonAsColor(JsonObjectGet(palette, "robot_chassis_gold"),
                       &pack->palette.robot_chassis_gold) &&
            JsonAsColor(JsonObjectGet(palette, "robot_limb_dark"),
                       &pack->palette.robot_limb_dark) &&
            JsonAsColor(JsonObjectGet(palette, "robot_skin_bronze"),
                       &pack->palette.robot_skin_bronze);
        if (!ok) {
            (void)snprintf(error, error_capacity,
                           "%s: \"palette\" is present but incomplete or "
                           "malformed (near \"%s\")", manifest_path,
                           bad_field != NULL ? bad_field : "palette");
            JsonFree(root);
            return false;
        }
    }

    /* -- render_target (required) -- */
    const CcJsonValue *render_target = JsonObjectGet(root, "render_target");
    static const char *const render_target_keys[] = {
        "width", "height", "upscale_filter",
    };
    if (render_target == NULL ||
        !JsonObjectKeysAllowed(render_target, render_target_keys, 3,
                              &unknown) ||
        !JsonAsInt(JsonObjectGet(render_target, "width"),
                  &pack->render_target_width) ||
        !JsonAsInt(JsonObjectGet(render_target, "height"),
                  &pack->render_target_height) ||
        pack->render_target_width <= 0 || pack->render_target_height <= 0) {
        (void)snprintf(error, error_capacity,
                       "%s: \"render_target\" needs positive integer "
                       "\"width\" and \"height\"", manifest_path);
        JsonFree(root);
        return false;
    }
    const CcJsonValue *upscale_filter =
        JsonObjectGet(render_target, "upscale_filter");
    if (JsonAsStringEquals(upscale_filter, "point")) {
        pack->render_target_upscale_filter = TEXTURE_FILTER_POINT;
    } else if (JsonAsStringEquals(upscale_filter, "bilinear")) {
        pack->render_target_upscale_filter = TEXTURE_FILTER_BILINEAR;
    } else {
        (void)snprintf(error, error_capacity,
                       "%s: \"render_target.upscale_filter\" must be "
                       "\"point\" or \"bilinear\"", manifest_path);
        JsonFree(root);
        return false;
    }

    /* -- post_chain (required, at least one pass, first must be "grade") -- */
    const CcJsonValue *post_chain = JsonObjectGet(root, "post_chain");
    if (post_chain == NULL || post_chain->type != CC_JSON_ARRAY ||
        post_chain->array_count < 1 ||
        post_chain->array_count > CC_STYLE_MAX_POST_PASSES) {
        (void)snprintf(error, error_capacity,
                       "%s: \"post_chain\" must be an array of 1 to %d "
                       "passes", manifest_path, CC_STYLE_MAX_POST_PASSES);
        JsonFree(root);
        return false;
    }
    static const char *const pass_keys[] = {
        "name", "shader", "inputs", "cache",
    };
    bool found_grade_pass = false;
    for (int32_t index = 0; index < post_chain->array_count; ++index) {
        const CcJsonValue *entry = post_chain->array_items[index];
        CcStylePostPass *pass = &pack->post_chain[index];
        *pass = (CcStylePostPass){0};
        if (entry == NULL || entry->type != CC_JSON_OBJECT ||
            !JsonObjectKeysAllowed(entry, pass_keys, 4, &unknown) ||
            !JsonAsString(JsonObjectGet(entry, "name"), pass->name,
                         sizeof(pass->name)) ||
            !JsonAsString(JsonObjectGet(entry, "shader"), pass->shader_path,
                         sizeof(pass->shader_path))) {
            (void)snprintf(error, error_capacity,
                           "%s: \"post_chain[%d]\" needs a \"name\" and a "
                           "\"shader\"", manifest_path, index);
            JsonFree(root);
            return false;
        }
        const CcJsonValue *inputs = JsonObjectGet(entry, "inputs");
        if (inputs == NULL || inputs->type != CC_JSON_ARRAY ||
            inputs->array_count < 1) {
            (void)snprintf(error, error_capacity,
                           "%s: \"post_chain[%d].inputs\" must be a "
                           "non-empty array", manifest_path, index);
            JsonFree(root);
            return false;
        }
        for (int32_t input_index = 0; input_index < inputs->array_count;
             ++input_index) {
            const CcJsonValue *input = inputs->array_items[input_index];
            if (JsonAsStringEquals(input, "scene_color")) {
                pass->input_scene_color = true;
            } else if (JsonAsStringEquals(input, "scene_depth")) {
                pass->input_scene_depth = true;
            } else if (JsonAsStringEquals(input, "scene_normal")) {
                pass->input_scene_normal = true;
            } else {
                (void)snprintf(error, error_capacity,
                               "%s: \"post_chain[%d].inputs[%d]\" must be "
                               "\"scene_color\", \"scene_depth\" or "
                               "\"scene_normal\"", manifest_path, index,
                               input_index);
                JsonFree(root);
                return false;
            }
        }
        const CcJsonValue *cache = JsonObjectGet(entry, "cache");
        if (cache == NULL || JsonAsStringEquals(cache, "per_frame")) {
            pass->cache = CC_STYLE_PASS_CACHE_PER_FRAME;
        } else if (JsonAsStringEquals(cache, "per_shot")) {
            /* Declared, not implemented -- see "Future passes" in
               docs/design/style-packs.md. */
            pass->cache = CC_STYLE_PASS_CACHE_PER_SHOT;
        } else {
            (void)snprintf(error, error_capacity,
                           "%s: \"post_chain[%d].cache\" must be "
                           "\"per_frame\" or \"per_shot\"", manifest_path,
                           index);
            JsonFree(root);
            return false;
        }
        if (strcmp(pass->name, "grade") == 0) {
            found_grade_pass = true;
            char resolved_grade[CC_STYLE_PATH_MAX];
            (void)snprintf(resolved_grade, sizeof(resolved_grade), "%s/%s",
                           pack_directory, pass->shader_path);
            if (strcmp(resolved_grade, pack->grade_fragment_shader) != 0) {
                (void)snprintf(error, error_capacity,
                               "%s: \"post_chain\"'s \"grade\" pass shader "
                               "must match \"shaders.grade_fragment\"",
                               manifest_path);
                JsonFree(root);
                return false;
            }
            (void)snprintf(pass->shader_path, sizeof(pass->shader_path),
                           "%s", pack->grade_fragment_shader);
        } else {
            char resolved_pass[CC_STYLE_PATH_MAX];
            (void)snprintf(resolved_pass, sizeof(resolved_pass), "%s/%s",
                           pack_directory, pass->shader_path);
            (void)snprintf(pass->shader_path, sizeof(pass->shader_path),
                           "%s", resolved_pass);
        }
    }
    if (!found_grade_pass) {
        (void)snprintf(error, error_capacity,
                       "%s: \"post_chain\" must include a pass named "
                       "\"grade\" (only the grade pass runs in this build)",
                       manifest_path);
        JsonFree(root);
        return false;
    }
    pack->post_chain_count = post_chain->array_count;
    if (pack->post_chain_count > 1) {
        TraceLog(LOG_INFO,
                 "STYLE: pack '%s' declares %d post passes; only \"grade\" "
                 "runs in this build",
                 id, pack->post_chain_count);
    }

    JsonFree(root);
    return true;
}

/* ---- public entry points ---------------------------------------------- */

void CcStylePackLoad(const char *requested_id)
{
    const char *id =
        (requested_id != NULL && requested_id[0] != '\0') ? requested_id
                                                           : "classic";
    CcStylePack pack;
    /* Generous on purpose: several error messages below embed two
       CC_STYLE_PATH_MAX (256-byte) strings plus explanatory text (for
       example "shaders.<role> points at a file that does not exist
       (<path>)"), and GCC's -Wformat-truncation reasons about the worst
       case a %s could produce, not the actual (much shorter, in practice)
       string -- a smaller buffer here builds fine under Clang but fails
       -Werror under GCC. */
    char error[1024] = {0};
    bool loaded_cleanly = CcStylePackParseFromDisk(id, &pack, error,
                                                   sizeof(error));
    if (!loaded_cleanly) {
        TraceLog(LOG_WARNING,
                 "STYLE: pack '%s' did not load (%s); falling back to the "
                 "compiled-in classic style",
                 id, error);
        CcStylePackResetToClassic(&pack);
        id = "classic";
    } else {
        TraceLog(LOG_INFO, "STYLE: loaded pack '%s' version %s", pack.id,
                 pack.version);
    }
    g_cc_active_style_pack = pack;
    g_cc_active_palette = pack.palette;
    CcSetActiveArtViewport(pack.render_target_width,
                           pack.render_target_height,
                           pack.render_target_upscale_filter);
    (void)snprintf(g_cc_active_style_pack_id, sizeof(g_cc_active_style_pack_id),
                   "%s", id);
    g_cc_active_style_pack_loaded_cleanly = loaded_cleanly;
}

const char *CcStylePackActiveId(void)
{
    return g_cc_active_style_pack_id;
}

bool CcStylePackLoadedCleanly(void)
{
    return g_cc_active_style_pack_loaded_cleanly;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
