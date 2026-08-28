#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D paletteLut;
uniform vec4 colDiffuse;
uniform float atmosphereExposure;
uniform float atmosphereShadowTone;
uniform float atmosphereHighlightTone;
uniform float atmosphereChroma;

out vec4 finalColor;

vec3 srgbToLinear(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), color));
}

vec3 linearToSrgb(vec3 color)
{
    vec3 low = color * 12.92;
    vec3 high = 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), color));
}

vec3 linearSrgbToOklab(vec3 color)
{
    float longWave = 0.4122214708 * color.r + 0.5363325363 * color.g +
                     0.0514459929 * color.b;
    float mediumWave = 0.2119034982 * color.r + 0.6806995451 * color.g +
                       0.1073969566 * color.b;
    float shortWave = 0.0883024619 * color.r + 0.2817188376 * color.g +
                      0.6299787005 * color.b;
    vec3 roots = pow(max(vec3(longWave, mediumWave, shortWave), vec3(0.0)),
                     vec3(1.0 / 3.0));
    return vec3(
        0.2104542553 * roots.x + 0.7936177850 * roots.y -
            0.0040720468 * roots.z,
        1.9779984951 * roots.x - 2.4285922050 * roots.y +
            0.4505937099 * roots.z,
        0.0259040371 * roots.x + 0.7827717662 * roots.y -
            0.8086757660 * roots.z);
}

vec3 oklabToLinearSrgb(vec3 color)
{
    float longRoot = color.x + 0.3963377774 * color.y +
                     0.2158037573 * color.z;
    float mediumRoot = color.x - 0.1055613458 * color.y -
                       0.0638541728 * color.z;
    float shortRoot = color.x - 0.0894841775 * color.y -
                      1.2914855480 * color.z;
    float longWave = longRoot * longRoot * longRoot;
    float mediumWave = mediumRoot * mediumRoot * mediumRoot;
    float shortWave = shortRoot * shortRoot * shortRoot;
    return vec3(
        4.0767416621 * longWave - 3.3077115913 * mediumWave +
            0.2309699292 * shortWave,
       -1.2684380046 * longWave + 2.6097574011 * mediumWave -
            0.3413193965 * shortWave,
       -0.0041960863 * longWave - 0.7034186147 * mediumWave +
            1.7076147010 * shortWave);
}

vec3 nearestPaletteColor(vec3 color)
{
    const float lookupMaximum = 63.0;
    ivec3 cell = ivec3(floor(clamp(color, 0.0, 1.0) *
                             lookupMaximum + 0.5));
    ivec2 address = ivec2(cell.r + (cell.b % 8) * 64,
                          cell.g + (cell.b / 8) * 64);
    return texelFetch(paletteLut, address, 0).rgb;
}

void main()
{
    /* Spatial pixelation is structural: the world is rendered into a true
       half-resolution target and enlarged with nearest-neighbor sampling.
       This pass only grades those already-stable art pixels. */
    vec4 source = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 perceptual = linearSrgbToOklab(
        srgbToLinear(clamp(source.rgb, 0.0, 1.0)));
    float originalLightness = perceptual.x;

    /* Work in a perceptual space. Shadows lose a little pigment, while the
       readable mid bands keep their authored hue. This avoids the old RGB
       contrast pass bending every material toward brown or green. */
    float pigment = mix(0.84, 1.02,
                         smoothstep(0.18, 0.58, originalLightness)) *
                     atmosphereChroma;
    perceptual.yz *= pigment;
    perceptual.x = clamp((perceptual.x - 0.46) * 1.045 + 0.46,
                         0.0, 1.0);
    perceptual.x = clamp(perceptual.x + atmosphereExposure, 0.0, 1.0);

    /* The core signature is a cool violet shadow and a restrained brass
       shoulder. The shift is intentionally small; material ramps still own
       the visible colors after lookup. */
    float shadowWeight = 1.0 - smoothstep(0.22, 0.50,
                                          originalLightness);
    float highlightWeight = smoothstep(0.58, 0.88, originalLightness);
    perceptual.y += shadowWeight * 0.004 * atmosphereShadowTone +
                    highlightWeight * 0.003 * atmosphereHighlightTone;
    perceptual.z += shadowWeight * -0.011 * atmosphereShadowTone +
                    highlightWeight * 0.012 * atmosphereHighlightTone;

    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float vignette = 1.0 - smoothstep(0.38, 1.28,
                                     dot(centered, centered));
    perceptual.x *= mix(0.94, 1.0, vignette);

    vec3 color = linearToSrgb(clamp(oklabToLinearSrgb(perceptual),
                                    0.0, 1.0));

    /* Fog has already been applied by the material shaders. Grading and the
       vignette happen above; this shared lookup is the final color operation
       so no blend or random grain can create off-palette pixels afterward. */
    color = nearestPaletteColor(clamp(color, 0.0, 1.0));
    /* The world target is always cleared to an opaque scene. Weather layers
       may use alpha while composing into it, but the presented world must be
       opaque or exact palette colors blend with the page background here. */
    finalColor = vec4(color, 1.0);
}
