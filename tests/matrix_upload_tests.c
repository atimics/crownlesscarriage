#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
EM_JS(void, PublishMatrixResult, (int failures), {
    globalThis.crownlessMatrixUpload = ({failures: failures, cases: 12});
});
#pragma clang diagnostic pop
#define GLSL_VERSION "#version 300 es\nprecision highp float;\n"
#else
#define GLSL_VERSION "#version 330\n"
#endif

static const char *vertex_source = GLSL_VERSION
    "in vec3 vertexPosition;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matrices[33];\n"
    "uniform vec2 weights;\n"
    "uniform int secondIndex;\n"
    "out vec4 result;\n"
    "void main() {\n"
    "  vec4 point = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "  vec4 posed = weights.x * matrices[0] * point +\n"
    "               weights.y * matrices[secondIndex] * point;\n"
    "  result = vec4((posed.xyz + 4.0)/8.0, posed.w/4.0);\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";
static const char *fragment_source = GLSL_VERSION
    "in vec4 result;\n"
    "out vec4 finalColor;\n"
    "void main() { finalColor = result; }\n";

static void DrawProbe(Shader shader, const Matrix *matrices, int count,
                      float first_weight, int second, int x)
{
    float weights[2] = {first_weight, 1.0f - first_weight};
    BeginShaderMode(shader);
    rlDrawRenderBatchActive();
    SetShaderValue(shader, GetShaderLocation(shader, "weights"), weights,
                   SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, GetShaderLocation(shader, "secondIndex"), &second,
                   SHADER_UNIFORM_INT);
    rlEnableShader(shader.id);
    rlSetUniformMatrices(GetShaderLocation(shader, "matrices"), matrices, count);
    DrawRectangle(x, 0, 32, 32, WHITE);
    EndShaderMode();
}

static int CheckPoint(Color actual, Vector3 expected, const char *name)
{
    float channels[4] = {(expected.x + 4.0f)*255.0f/8.0f,
                         (expected.y + 4.0f)*255.0f/8.0f,
                         (expected.z + 4.0f)*255.0f/8.0f, 255.0f/4.0f};
    unsigned char pixels[4] = {actual.r, actual.g, actual.b, actual.a};
    for (int i = 0; i < 4; ++i) {
        if (fabsf((float)pixels[i] - channels[i]) > 1.1f) {
            (void)fprintf(stderr, "%s channel %d: got %u, expected %.2f\n",
                          name, i, (unsigned int)pixels[i], (double)channels[i]);
            return 1;
        }
    }
    (void)printf("PASS matrix upload: %s\n", name);
    return 0;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 32, "Matrix upload readback");
    if (!IsWindowReady()) return 1;
    Shader shader = LoadShaderFromMemory(vertex_source, fragment_source);
    RenderTexture2D target = LoadRenderTexture(64, 32);
    if (!IsShaderValid(shader) || !IsRenderTextureValid(target)) return 1;
    /* Read shader output directly, including homogeneous w. */
    rlDisableColorBlend();
    int failures = 0;
    const char *names[] = {"identity", "translation +2", "translation -1",
                           "asymmetric rotation", "weighted bones", "33 matrices"};
    Matrix matrices[33];
    for (int i = 0; i < 33; ++i) matrices[i] = MatrixIdentity();
    for (int test = 0; test < 6; ++test) {
        matrices[0] = MatrixIdentity();
        matrices[1] = MatrixIdentity();
        Vector3 expected = {1, 0, 0};
        int count = 2, second = 1;
        float weight = 1.0f;
        if (test == 1) { matrices[0] = MatrixTranslate(2, 0, 0); expected.x = 3; }
        if (test == 2) { matrices[0] = MatrixTranslate(-1, 0, 0); expected.x = 0; }
        if (test == 3) {
            matrices[0] = MatrixRotateZ(0.6f);
            expected = (Vector3){cosf(0.6f), sinf(0.6f), 0};
        }
        if (test >= 4) {
            matrices[0] = MatrixTranslate(2, 0, 0);
            second = test == 5 ? 32 : 1;
            count = test == 5 ? 33 : 2;
            matrices[second] = MatrixRotateZ(0.6f);
            weight = 0.25f;
            expected = (Vector3){0.75f + 0.75f*cosf(0.6f), 0.75f*sinf(0.6f), 0};
        }
        BeginTextureMode(target);
        ClearBackground(BLANK);
        DrawProbe(shader, matrices, count, weight, second, 0);
        /* A second actor in the same frame must retain its separate pose. */
        Matrix other[2] = {MatrixTranslate(-1, 1, 0), MatrixIdentity()};
        DrawProbe(shader, other, 2, 1.0f, 1, 32);
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        failures += CheckPoint(GetImageColor(image, 16, 16), expected, names[test]);
        failures += CheckPoint(GetImageColor(image, 48, 16), (Vector3){0, 1, 0}, "second actor");
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
    UnloadShader(shader);
#if defined(__EMSCRIPTEN__)
    PublishMatrixResult(failures);
#endif
    CloseWindow();
    return failures > 0 ? 1 : 0;
}
