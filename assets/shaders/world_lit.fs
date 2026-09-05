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
uniform float weatherWetness;
uniform float horizonFog;

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

float perceivedGray(vec3 color)
{
    vec3 linear = srgbToLinear(clamp(color, 0.0, 1.0));
    float luminance = dot(linear, vec3(0.2126, 0.7152, 0.0722));
    return linearToSrgb(luminance);
}

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

float centeredStripe(float coordinate, float frequency, float halfWidth)
{
    float phase = fract(coordinate * frequency);
    return 1.0 - step(halfWidth, min(phase, 1.0 - phase));
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
    if (isTerrain > 0.5 && horizonFog > 0.5) {
        // The travel landscape uses broad paint and distance haze.
        float brush = cellNoise(fragPosition.xz * 0.8) - 0.5;
        vec3 light = ambientColor * 0.90 + lightColor * (key * 0.44 + 0.12);
        vec3 paint = albedo.rgb * light * (1.0 + brush * 0.07);
        paint *= mix(shadowColor, vec3(1.0), smoothstep(-0.08, 0.58, facing));
        float haze = smoothstep(fogNear, fogFar, viewDepth);
        finalColor = vec4(mix(clamp(paint, 0.0, 1.0), fogColor, haze), albedo.a);
        return;
    }
    float foregroundBand = 1.0 - smoothstep(depthSplits.x,
                                             depthSplits.y, viewDepth);
    float backgroundBand = smoothstep(depthSplits.y,
                                      depthSplits.z, viewDepth);
    float foregroundMass = 1.0 - smoothstep(
        2.5, 8.0, distance(fragPosition.xz, foregroundAnchor.xz));
    float foreground = max(foregroundBand, foregroundMass * 0.55);
    float detailPresence = 1.0 - backgroundBand * depthStrength * 0.86;

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

    float terrainHemisphere = hemisphere < 0.72 ? 0.64 :
                              hemisphere < 0.91 ? 0.82 : 1.0;
    float paintedHemisphere = mix(hemisphere, terrainHemisphere, isTerrain);
    vec3 light = ambientColor * mix(0.76, 1.04, paintedHemisphere);
    light += vec3(0.13, 0.19, 0.21) * smoothstep(-0.35, 0.85, normal.y) *
             0.10;
    light += vec3(0.19, 0.11, 0.065) * max(-normal.y, 0.0) * 0.07;
    float smoothDiffuse = key * 0.48 + wrap * 0.20;
    float diffuse = smoothDiffuse;
    float paintedDiffuse = diffuse < 0.16 ? 0.06 :
                           diffuse < 0.36 ? 0.28 :
                           diffuse < 0.54 ? 0.52 : 0.76;
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

    float blockVariation = hash21(floor(fragPosition.xz * 0.48)) - 0.5;
    float structureVertical = 1.0 - abs(normal.y);
    vec2 facadeCoordinate = abs(normal.x) > abs(normal.z) ?
                            fragPosition.zy : fragPosition.xy;
    float facadePatch = cellNoise(
        facadeCoordinate * vec2(0.34, 0.48) + vec2(7.0, 19.0)) - 0.5;
    float facadeChip = cellNoise(
        facadeCoordinate * vec2(0.92, 0.22) + vec2(31.0, 5.0)) - 0.5;
    float facadeVariation = (facadePatch * 0.090 + facadeChip * 0.024) *
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

    float foregroundTerrain = foreground * isTerrain * detailPresence;
    float foregroundScumble = cellNoise(
        crossedTerrainPoint * 0.17 + vec2(73.0, 29.0));
    float foregroundShade = 1.0 - step(0.34, foregroundScumble);
    float foregroundLift = step(0.72, foregroundScumble);
    color *= 1.0 - foregroundTerrain * foregroundShade * 0.065;
    color += albedo.rgb * vec3(0.12, 0.14, 0.12) *
             foregroundTerrain * foregroundLift * 0.16;

    if (isTerrain > 0.5) {
        vec2 groundMarkPoint = crossedTerrainPoint * 2.08 +
                               vec2(5.0, 17.0);
        vec2 groundMarkCenter = abs(fract(groundMarkPoint) - 0.5);
        float groundMarkShape = 1.0 - step(
            0.29, groundMarkCenter.x + groundMarkCenter.y * 1.34);
        float groundMark = detailPresence * groundMarkShape *
            step(0.90,
                 hash21(floor(groundMarkPoint) + vec2(37.0, 61.0)));
        color = mix(color, color * vec3(0.82, 0.86, 0.84),
                    groundMark * 0.18);
    } else {
        float albedoValue = perceivedGray(albedo.rgb);
        float normalAxis = step(abs(normal.z), abs(normal.x));
        float pitchedSurface =
            smoothstep(0.24, 0.58, normal.y) *
            (1.0 - smoothstep(0.94, 0.995, normal.y)) *
            (1.0 - smoothstep(0.50, 0.64, albedoValue));
        float roofAcross = mix(fragPosition.z, fragPosition.x, normalAxis);
        float roofAlong = mix(fragPosition.x, fragPosition.z, normalAxis);
        float roofCourseCoordinate = abs(roofAcross) * 1.38;
        float roofCourseIndex = floor(roofCourseCoordinate);
        float roofCoursePhase = fract(roofCourseCoordinate);
        float roofCourse = centeredStripe(abs(roofAcross), 1.38, 0.085);
        float roofCourseBreak = step(
            0.24, hash21(vec2(floor(roofAlong * 0.78),
                              roofCourseIndex + 17.0)));
        float roofJointPhase = fract(roofAlong * 0.78 +
                                     mod(roofCourseIndex, 2.0) * 0.5);
        float roofJoint = (1.0 - step(0.065,
                                      min(roofJointPhase,
                                          1.0 - roofJointPhase))) *
                          (1.0 - roofCourse);
        float roofJointBreak = step(
            0.34, hash21(vec2(roofCourseIndex + 53.0,
                              floor(roofAlong * 0.78))));
        float roofInk = pitchedSurface * detailPresence *
            max(roofCourse * roofCourseBreak * 0.72,
                roofJoint * roofJointBreak * 0.52);
        float roofLip = pitchedSurface * detailPresence * roofCourseBreak *
            step(0.09, roofCoursePhase) *
            (1.0 - step(0.18, roofCoursePhase));

        // Warm timber uses long grain; stone keeps its broken mortar courses.
        float timber = smoothstep(0.025, 0.11, albedo.r - albedo.g) *
                       (1.0 - smoothstep(0.40, 0.62, albedoValue));
        vec2 grainPoint = facadeCoordinate * vec2(6.0, 0.42);
        float grain = cellNoise(grainPoint + vec2(17.0, 43.0));
        float grainResolution = 1.0 - smoothstep(
            0.35, 1.10, max(fwidth(grainPoint.x), fwidth(grainPoint.y)));
        float timberSurface = timber * structureVertical * detailPresence;
        color *= 1.0 + (grain - 0.5) * timberSurface *
                         grainResolution * 0.16;

        float wallSurface = structureVertical * (1.0 - timber) *
                            smoothstep(0.12, 0.34, albedoValue);
        float wallAlong = mix(fragPosition.x, fragPosition.z, normalAxis);
        float wallCourseCoordinate = max(fragPosition.y, 0.0) * 1.08;
        float wallCourseIndex = floor(wallCourseCoordinate);
        float wallCourse = centeredStripe(fragPosition.y, 1.08, 0.052);
        float wallCell = floor(wallAlong * 0.48 +
                               mod(wallCourseIndex, 2.0) * 0.5);
        float wallCourseBreak = step(
            0.49, hash21(vec2(wallCell, wallCourseIndex + 71.0)));
        float wallJointPhase = fract(wallAlong * 0.48 +
                                     mod(wallCourseIndex, 2.0) * 0.5);
        float wallJoint = 1.0 - step(
            0.052, min(wallJointPhase, 1.0 - wallJointPhase));
        float wallJointBreak = step(
            0.70,
            hash21(vec2(wallCourseIndex + 31.0, wallCell + 11.0)));
        float wallInk = wallSurface * detailPresence *
            max(wallCourse * wallCourseBreak * 0.46,
                wallJoint * wallJointBreak * 0.34);

        vec2 wallChipPoint = vec2(wallAlong, fragPosition.y) * 1.72;
        vec2 wallChipCenter = abs(fract(wallChipPoint) - 0.5);
        float wallChipShape = 1.0 - step(
            0.42, wallChipCenter.x + wallChipCenter.y * 1.22);
        float wallChip = wallSurface * detailPresence * wallChipShape *
            step(0.88,
                 hash21(floor(wallChipPoint) + vec2(89.0, 13.0)));

        vec3 materialInk = mix(vec3(0.020, 0.031, 0.034),
                               albedo.rgb * 0.24, 0.38);
        color = mix(color, materialInk,
                    clamp(roofInk * 0.34 + wallInk * 0.24 +
                          wallChip * 0.18, 0.0, 0.44));
        color += albedo.rgb * vec3(1.14, 1.08, 0.88) *
                 roofLip * 0.040;
    }

    color += lightColor * specular * mix(1.0, 0.12, isTerrain) *
             detailPresence;
    float wetSurface = weatherWetness * smoothstep(0.58, 0.94, normal.y);
    float wetSheen = pow(max(dot(normal, halfDirection), 0.0), 8.0) *
                     smoothstep(0.02, 0.64, key) * wetSurface;
    color *= mix(1.0, 0.86, wetSurface);
    color += lightColor * wetSheen * 0.13 * detailPresence;
    color += vec3(0.20, 0.46, 0.48) * rim *
             mix(0.052, 0.018, isTerrain) * detailPresence;

    float verticalSurface = structureVertical;
    float foundation = (1.0 - smoothstep(0.04, 0.92, fragPosition.y)) *
                       verticalSurface * (1.0 - isTerrain);
    color *= mix(1.0, 0.79, foundation);

    float viewFacing = abs(dot(normal, viewDirection));
    float edgeInk = (1.0 - step(0.13, viewFacing)) * (1.0 - isTerrain);
    vec3 coloredInk = mix(vec3(0.020, 0.031, 0.034),
                          albedo.rgb * 0.20, 0.28);
    color = mix(color, coloredInk, edgeInk * 0.72 * detailPresence);

    float fragmentCameraDistance = viewDepth;
    float revealAmount = clamp(foregroundReveal, 0.0, 1.0);
    if (revealAmount > 0.001) {
        float activeCutHeight = mix(revealCutHeight + 8.0,
                                    revealCutHeight, revealAmount);
        if (fragPosition.y > activeCutHeight) discard;
        float belowCut = step(fragPosition.y, activeCutHeight);
        float cutBand = (1.0 - smoothstep(0.02, 0.13,
                                          activeCutHeight - fragPosition.y)) *
                        belowCut;
        color = mix(color, coloredInk, cutBand * 0.72 * revealAmount);
    }
    float backgroundWeight = backgroundBand * depthStrength;
    float luminance = perceivedGray(color);
    vec3 quietBackground = mix(vec3(luminance) * vec3(0.84, 0.94, 1.06),
                               fogColor + vec3(0.055), 0.42);
    color = mix(color, quietBackground, backgroundWeight * 0.48);
    float foregroundDarkening = mix(0.10, 0.055, isTerrain);
    color *= 1.0 - foreground * depthStrength * foregroundDarkening;

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
                mix(mix(0.22, 0.42, depthStrength), 1.0, horizonFog);
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
