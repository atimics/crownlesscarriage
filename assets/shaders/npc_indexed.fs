#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 characterPalette[9];
uniform float paletteInk[9];
uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 shadowColor;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float inkStrength;
uniform float heroEmphasis;
uniform vec3 heroHeadPosition;
uniform float bodySkinRemap;
uniform vec3 characterRimTint;
uniform float characterRimStrength;
uniform float materialStyle;

out vec4 finalColor;

// Skin, cloth, leather, hair, metal, eye: dark, light, gloss, highlight.
vec4 surfaceResponse(int surface)
{
    if (surface == 0) return vec4(0.78, 1.04, 8.0, 0.016);
    if (surface == 1) return vec4(0.65, 1.00, 6.0, 0.006);
    if (surface == 2) return vec4(0.58, 1.02, 12.0, 0.055);
    if (surface == 3) return vec4(0.55, 0.94, 20.0, 0.045);
    if (surface == 4) return vec4(0.48, 1.08, 24.0, 0.26);
    return vec4(0.50, 0.95, 12.0, 0.005);
}

void main()
{
    int paletteIndex = clamp(int(floor(fragColor.r * 9.0)), 0, 8);
    vec4 paint = characterPalette[paletteIndex];
    if (bodySkinRemap > 0.5 && paletteIndex == 0) {
        paletteIndex = fragPosition.y < heroHeadPosition.y - 0.82 ? 4 : 2;
        paint = characterPalette[paletteIndex];
    }
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    vec3 toCamera = normalize(cameraPosition - fragPosition);
    vec3 headDelta = fragPosition - heroHeadPosition;
    float headFocus = exp2(-dot(headDelta, headDelta) * 9.0) * heroEmphasis;
    float facing = dot(normal, toLight);
    float wrapped = clamp((facing + 0.32) / 1.32, 0.0, 1.0);
    float lightBand = step(0.48, wrapped);
    lightBand = max(lightBand, step(0.30, headFocus));

    int surface = paletteIndex == 0 ? 0 : paletteIndex == 1 ? 3 :
                  paletteIndex == 5 ? 2 :
                  (paletteIndex == 6 || paletteIndex == 7) ? 4 :
                  paletteIndex == 8 ? 5 : 1;
    int authoredSurface = int(floor(fragColor.a * 8.0));
    if (authoredSurface < 6) surface = authoredSurface;
    vec4 response = surfaceResponse(surface);
    bool isSkin = materialStyle > 0.5 ? surface == 0 : paletteIndex == 0;
    float darkValue = isSkin ? 0.72 : 0.63;
    float lightValue = isSkin ? 1.04 : 1.01;
    darkValue = mix(darkValue, response.x, materialStyle);
    lightValue = mix(lightValue, response.y, materialStyle);
    float authoredValue = fragColor.g < 0.375 ? (isSkin ? 0.73 : 0.64) :
                          fragColor.g < 0.625 ? (isSkin ? 0.88 : 0.82) :
                                                     (isSkin ? 1.05 : 1.03);
    vec3 shadowTemperature = isSkin ? vec3(1.04, 0.84, 0.77) :
                                      vec3(0.83, 0.91, 1.02);
    shadowTemperature *= shadowColor;
    vec3 lightTemperature = vec3(1.035, 1.01, 0.95);
    vec3 temperature = mix(shadowTemperature, lightTemperature, lightBand);
    float normalValue = mix(darkValue, lightValue, lightBand);
    float authoredWeight = mix(0.72, 0.46, heroEmphasis);
    authoredWeight = mix(authoredWeight, surface == 4 ? 0.28 : 0.58,
                         materialStyle);
    vec3 color = paint.rgb * mix(normalValue, authoredValue, authoredWeight) *
                 temperature;
    float foldShadow = (1.0 - smoothstep(-0.18, 0.48, facing)) *
                       fragColor.b;
    color *= 1.0 - foldShadow * 0.11;

    float skyExposure = smoothstep(-0.30, 0.82, normal.y);
    color += paint.rgb * vec3(0.74, 0.91, 1.04) * skyExposure *
             (1.0 - lightBand) * 0.052;

    vec3 halfDirection = normalize(toCamera + toLight);
    float gloss = pow(max(dot(normal, halfDirection), 0.0), response.z);
    float glossWidth = max(fwidth(gloss), 0.055);
    float highlight = smoothstep(0.52 - glossWidth, 0.52 + glossWidth, gloss);
    vec3 highlightColor = surface == 4 ? mix(vec3(1.0), paint.rgb, 0.30) :
                                          vec3(1.0, 0.91, 0.80);
    color += highlightColor * highlight * response.w * lightBand * materialStyle;

    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - smoothstep(0.055, 0.205, viewFacing);
    vec3 coloredInk = mix(vec3(0.024, 0.030, 0.032),
                          paint.rgb * 0.24, 0.38);
    color = mix(color, coloredInk,
                edgeInk * inkStrength * paletteInk[paletteIndex] *
                mix(1.0, surface == 0 ? 0.72 : 0.90, materialStyle));
    float litEdge = edgeInk * smoothstep(0.08, 0.68, facing) * lightBand;
    color += vec3(0.18, 0.27, 0.28) * litEdge * 0.050;

    float silhouette = smoothstep(0.70, 0.94, 1.0 - viewFacing);
    float castRim = silhouette * (0.020 + heroEmphasis * 0.050) *
                    (0.58 + skyExposure * 0.42) * characterRimStrength;
    float heroRim = silhouette * heroEmphasis * headFocus * 0.070 *
                    characterRimStrength;
    vec3 castRimColor = mix(characterRimTint, paint.rgb, 0.22);
    color += castRimColor * (castRim + heroRim);
    color *= 1.0 + headFocus * 0.075;

    float distanceToCamera = length(cameraPosition - fragPosition);
    float fog = smoothstep(fogNear, fogFar, distanceToCamera) * 0.24;
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), paint.a * colDiffuse.a);
}
