#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D paletteLut;
uniform vec4 colDiffuse;

out vec4 finalColor;

vec3 srgbToLinear(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), color));
}

float linearToSrgb(float value)
{
    return value <= 0.0031308 ? value * 12.92 :
           1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

float relativeLuminance(vec3 color)
{
    vec3 linear = srgbToLinear(clamp(color, 0.0, 1.0));
    return dot(linear, vec3(0.2126, 0.7152, 0.0722));
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
    vec3 color = source.rgb;
    float luminance = relativeLuminance(color);
    float gray = linearToSrgb(luminance);

    /* Keep shadows restrained while letting mid-value costume and faction
       pigments carry the frame. A gentle contrast curve adds depth without
       crushing the inked silhouettes. */
    float pigment = mix(0.86, 0.98, smoothstep(0.02, 0.34, luminance));
    color = mix(vec3(gray), color, pigment);
    color = (color - 0.46) * 1.055 + 0.46;
    color = max(color, vec3(0.0));

    /* Split-toning binds procedural shapes and authored glTF assets: cool
       air opens low values and warm light gives the brightest planes a
       parchment-gold shoulder instead of clipping toward white. */
    float shadowWeight = 1.0 - smoothstep(0.015, 0.22, luminance);
    float highlightWeight = smoothstep(0.20, 0.62, luminance);
    color += vec3(0.006, 0.015, 0.020) * shadowWeight;
    color *= mix(vec3(1.0), vec3(1.035, 1.012, 0.958), highlightWeight);
    color = color / (vec3(0.965) + color * 0.055);

    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float vignette = 1.0 - smoothstep(0.38, 1.28,
                                     dot(centered, centered));
    color *= mix(0.915, 1.018, vignette);

    /* Fog has already been applied by the material shaders. Grading and the
       vignette happen above; this shared lookup is the final color operation
       so no blend or random grain can create off-palette pixels afterward. */
    color = nearestPaletteColor(clamp(color, 0.0, 1.0));
    finalColor = vec4(color, source.a);
}
