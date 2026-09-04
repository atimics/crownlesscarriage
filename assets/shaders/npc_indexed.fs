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

out vec4 finalColor;

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

    bool isSkin = paletteIndex == 0;
    float darkValue = isSkin ? 0.72 : 0.63;
    float lightValue = isSkin ? 1.04 : 1.01;
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
    vec3 color = paint.rgb * mix(normalValue, authoredValue, authoredWeight) *
                 temperature;
    float foldShadow = (1.0 - smoothstep(-0.18, 0.48, facing)) *
                       fragColor.b;
    color *= 1.0 - foldShadow * 0.11;

    float skyExposure = smoothstep(-0.30, 0.82, normal.y);
    color += paint.rgb * vec3(0.74, 0.91, 1.04) * skyExposure *
             (1.0 - lightBand) * 0.052;

    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - smoothstep(0.055, 0.205, viewFacing);
    vec3 coloredInk = mix(vec3(0.024, 0.030, 0.032),
                          paint.rgb * 0.24, 0.38);
    color = mix(color, coloredInk,
                edgeInk * inkStrength * paletteInk[paletteIndex]);
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
