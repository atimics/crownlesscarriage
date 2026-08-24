#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 shadowColor;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform vec3 focalPoint;
uniform vec2 storyAxis;
uniform vec3 foregroundAnchor;
uniform vec3 depthSplits;
uniform float depthStrength;
uniform float focalContrast;
uniform float foregroundReveal;
uniform float revealCutHeight;
uniform float terrainSurface;

out vec4 finalColor;

float hash21(vec2 point)
{
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
}

float cellNoise(vec2 point)
{
    return hash21(floor(point));
}

float orderedDither4x4(vec2 pixel)
{
    ivec2 cell = ivec2(mod(floor(pixel), 4.0));
    int index = cell.x + cell.y * 4;
    const float threshold[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0
    );
    return (threshold[index] + 0.5) / 16.0;
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    float facing = dot(normal, toLight);
    float key = max(facing, 0.0);
    float wrap = clamp((facing + 0.32) / 1.32, 0.0, 1.0);
    float hemisphere = normal.y * 0.5 + 0.5;
    float isTerrain = step(0.5, terrainSurface);
    float viewDepth = max(0.0, dot(fragPosition - cameraPosition,
                                   normalize(cameraForward)));
    float foregroundBand = 1.0 - smoothstep(depthSplits.x,
                                             depthSplits.y, viewDepth);
    float backgroundBand = smoothstep(depthSplits.y,
                                      depthSplits.z, viewDepth);
    float foregroundMass = 1.0 - smoothstep(
        2.5, 8.0, distance(fragPosition.xz, foregroundAnchor.xz));
    float foreground = max(foregroundBand, foregroundMass * 0.55);
    float detailPresence = 1.0 - backgroundBand * depthStrength * 0.86;

    /* Land is painted as stacked, hard-edged material layers. Two crossed
       grids keep the shapes irregular without interpolating between samples;
       a third, tighter grid adds sparse pixel-sized chips. */
    vec2 terrainPoint = fragPosition.xz;
    vec2 crossedTerrainPoint = vec2(
        terrainPoint.x + terrainPoint.y * 0.46,
        terrainPoint.y - terrainPoint.x * 0.29);
    float broadTerrain = mix(
        cellNoise(terrainPoint * 0.18 + vec2(11.0, 5.0)),
        cellNoise(crossedTerrainPoint * 0.13 + vec2(3.0, 19.0)),
        0.38);
    float middleTerrain = mix(
        cellNoise(terrainPoint * 0.55 + vec2(23.0, 7.0)),
        cellNoise(crossedTerrainPoint * 0.39 + vec2(5.0, 31.0)),
        0.34);
    float fineTerrain = cellNoise(
        crossedTerrainPoint * 2.35 + vec2(41.0, 13.0));
    float terrainShadowLayer = 1.0 - step(0.29, broadTerrain);
    float terrainLightLayer = step(0.70, broadTerrain);
    float terrainCoolLayer = step(0.76, middleTerrain);
    float terrainDarkChip = 1.0 - step(0.10, fineTerrain);
    float terrainLightChip = step(0.88, fineTerrain);

    /* A cool sky fill and a faint warm ground bounce keep all three major
       plane families distinct. This is deliberately broad lighting rather
       than a smooth miniature render: it survives the final art-pixel grid. */
    float terrainHemisphere = hemisphere < 0.72 ? 0.64 :
                              hemisphere < 0.91 ? 0.82 : 1.0;
    float paintedHemisphere = mix(hemisphere, terrainHemisphere, isTerrain);
    vec3 light = ambientColor * mix(0.76, 1.04, paintedHemisphere);
    light += vec3(0.13, 0.19, 0.21) * smoothstep(-0.35, 0.85, normal.y) *
             0.10;
    light += vec3(0.19, 0.11, 0.065) * max(-normal.y, 0.0) * 0.07;
    float smoothDiffuse = key * 0.48 + wrap * 0.20;
    float diffuse = smoothDiffuse;
    /* Three explicit paint values give roofs, facades, and inset planes a
       stronger hierarchy without adding noisy texture detail. */
    float paintedDiffuse = diffuse < 0.18 ? 0.0 :
                           diffuse < 0.48 ? 0.34 : 0.68;
    float terrainDiffuse = smoothDiffuse < 0.18 ? 0.08 :
                           smoothDiffuse < 0.40 ? 0.30 :
                           smoothDiffuse < 0.58 ? 0.49 : 0.67;
    diffuse = mix(paintedDiffuse, terrainDiffuse, isTerrain);
    light += lightColor * diffuse;

    vec3 viewDirection = normalize(cameraPosition - fragPosition);
    vec3 halfDirection = normalize(viewDirection + toLight);
    float specular = pow(max(dot(normal, halfDirection), 0.0), 30.0) *
                     smoothstep(0.10, 0.70, key) * 0.11;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);

    /* Low-frequency value breakup keeps large procedural surfaces from
       reading as single-color slabs without introducing noisy textures. */
    float blockVariation = hash21(floor(fragPosition.xz * 0.48)) - 0.5;
    float structureVertical = 1.0 - abs(normal.y);
    vec2 facadeCoordinate = abs(normal.x) > abs(normal.z) ?
                            fragPosition.zy : fragPosition.xy;
    float facadePatch = cellNoise(
        facadeCoordinate * vec2(0.34, 0.48) + vec2(7.0, 19.0)) - 0.5;
    float facadeChip = cellNoise(
        facadeCoordinate * vec2(0.92, 0.22) + vec2(31.0, 5.0)) - 0.5;
    float facadeVariation = (facadePatch * 0.050 + facadeChip * 0.018) *
                            structureVertical * (1.0 - isTerrain);
    float variation = blockVariation * 0.026 * (1.0 - isTerrain);
    variation += facadeVariation;
    variation *= detailPresence;
    vec3 paintTemperature = mix(shadowColor, vec3(1.0),
                                smoothstep(-0.08, 0.58, facing));
    vec3 color = albedo.rgb * light * paintTemperature * (1.0 + variation);

    vec3 terrainTint = vec3(1.0);
    terrainTint += vec3(-0.060, -0.042, -0.018) * terrainShadowLayer;
    terrainTint += vec3(0.058, 0.036, -0.026) * terrainLightLayer;
    terrainTint += vec3(-0.032, 0.016, 0.014) * terrainCoolLayer;
    terrainTint += vec3(-0.035, -0.026, -0.012) * terrainDarkChip;
    terrainTint += vec3(0.034, 0.026, -0.010) * terrainLightChip;
    color *= mix(vec3(1.0), terrainTint,
                 isTerrain * detailPresence);
    color += lightColor * specular * mix(1.0, 0.12, isTerrain) *
             detailPresence;
    color += vec3(0.20, 0.46, 0.48) * rim *
             mix(0.052, 0.018, isTerrain) * detailPresence;

    /* Darken the foot of vertical structures where they meet the terrain.
       This inexpensive contact cue grounds walls, carts, and scenery without
       requiring a crawling screen-space shadow pass. Horizontal ground and
       roof planes remain untouched. */
    float verticalSurface = structureVertical;
    float foundation = (1.0 - smoothstep(0.04, 0.92, fragPosition.y)) *
                       verticalSurface * (1.0 - isTerrain);
    color *= mix(1.0, 0.79, foundation);

    /* A view-stable colored edge keeps large structural silhouettes legible
       without a screen-space outline pass that would crawl between pixels. */
    float viewFacing = abs(dot(normal, viewDirection));
    float edgeInk = (1.0 - step(0.13, viewFacing)) * (1.0 - isTerrain);
    vec3 coloredInk = mix(vec3(0.020, 0.031, 0.034),
                          albedo.rgb * 0.20, 0.28);
    color = mix(color, coloredInk, edgeInk * 0.72 * detailPresence);

    /* Foreground classification happens per complete house on the CPU. A
       selected house becomes a stable waist-high architectural cutaway: roof,
       upper wall, and trim all leave together while the grounded lower shell
       remains. Only the narrow cut edge dithers, so the reveal never becomes
       a large screen-space bubble around the hero. */
    float fragmentCameraDistance = viewDepth;
    float revealAmount = clamp(foregroundReveal, 0.0, 1.0);
    if (revealAmount > 0.001) {
        float coverage = smoothstep(revealCutHeight - 0.10,
                                    revealCutHeight + 0.08,
                                    fragPosition.y);
        float screenDither = orderedDither4x4(gl_FragCoord.xy);
        if (screenDither < coverage * revealAmount) discard;
        float belowCut = step(fragPosition.y, revealCutHeight);
        float cutBand = (1.0 - smoothstep(0.02, 0.13,
                                          revealCutHeight - fragPosition.y)) *
                        belowCut;
        color = mix(color, coloredInk, cutBand * 0.72 * revealAmount);
    }
    /* Camera-forward bands make distance a designed layer instead of a side
       effect of radial fog. The far layer loses chips and ink, cools, and
       moves toward the background value. A nearby framing mass is darker. */
    float backgroundWeight = backgroundBand * depthStrength;
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    vec3 quietBackground = mix(vec3(luminance) * vec3(0.84, 0.94, 1.06),
                               fogColor + vec3(0.055), 0.42);
    color = mix(color, quietBackground, backgroundWeight * 0.48);
    color *= 1.0 - foreground * depthStrength * 0.16;

    vec2 axis = length(storyAxis) > 0.001 ? normalize(storyAxis) :
                                            vec2(1.0, 0.0);
    vec2 fromFocus = fragPosition.xz - focalPoint.xz;
    float acrossAxis = abs(fromFocus.x * axis.y - fromFocus.y * axis.x);
    float alongAxis = abs(dot(fromFocus, axis));
    float focusCore = 1.0 - smoothstep(2.0, 9.0, length(fromFocus));
    float leadingLine = (1.0 - smoothstep(1.5, 5.5, acrossAxis)) *
                        (1.0 - smoothstep(7.0, 28.0, alongAxis));
    float focusWeight = max(focusCore, leadingLine * 0.52) *
                        (1.0 - backgroundBand * 0.55);
    color = (color - vec3(0.42)) *
            (1.0 + focusWeight * focalContrast) + vec3(0.42);

    float fog = smoothstep(fogNear, fogFar, fragmentCameraDistance) *
                mix(0.22, 0.42, depthStrength);
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
