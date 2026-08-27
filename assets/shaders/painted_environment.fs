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

out vec4 finalColor;

float hash21(vec2 point)
{
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
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
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse;
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    vec3 toCamera = normalize(cameraPosition - fragPosition);
    float facing = dot(normal, toLight);

    /* COLOR_0 is an art contract, not a tint: red is broad value, green is
       material class, and blue reserves one story accent. */
    float authoredValue = clamp(fragColor.r, 0.0, 1.0);
    float materialClass = clamp(fragColor.g, 0.0, 1.0);
    float accent = clamp(fragColor.b, 0.0, 1.0);
    float valueBand = authoredValue < 0.40 ? 0.64 :
                      authoredValue < 0.62 ? 0.84 : 1.06;
    float weakNormalBand = facing < 0.05 ? 0.92 :
                           facing < 0.55 ? 1.00 : 1.07;
    vec3 temperature = mix(shadowColor, vec3(1.0),
                           smoothstep(-0.08, 0.62, facing));
    vec3 color = surface.rgb * valueBand * weakNormalBand * temperature;
    color *= ambientColor + lightColor * max(facing, 0.0) * 0.24;

    float cloth = 1.0 - step(0.20, materialClass);
    float wood = step(0.20, materialClass) *
                 (1.0 - step(0.40, materialClass));
    float stone = step(0.40, materialClass) *
                  (1.0 - step(0.60, materialClass));
    float metal = step(0.60, materialClass) *
                  (1.0 - step(0.80, materialClass));
    float water = step(0.80, materialClass);

    /* Each material gets one large, readable paint gesture. */
    float broadCloth = step(0.20, normal.y) * cloth;
    color *= mix(vec3(1.0), vec3(1.035, 1.015, 0.96), broadCloth * 0.22);

    float woodStroke = smoothstep(0.10, 0.88,
                                  abs(dot(normal, vec3(0.82, 0.0, 0.57))));
    color += surface.rgb * vec3(0.18, 0.095, 0.025) *
             wood * woodStroke * 0.16;

    float stoneFleck = step(0.78, hash21(floor(fragPosition.xz * 1.15))) *
                       smoothstep(0.20, 0.86, normal.y);
    color += vec3(0.075, 0.085, 0.080) * stone * stoneFleck;

    vec3 halfDirection = normalize(toCamera + toLight);
    float metalStroke = step(0.82, pow(max(dot(normal, halfDirection), 0.0),
                                       18.0));
    color += lightColor * metal * metalStroke * (0.16 + accent * 0.14);

    float waterStroke = step(0.62, hash21(
        floor(vec2(fragPosition.x * 0.72, fragPosition.z * 3.0))));
    color += vec3(0.035, 0.11, 0.12) * water * waterStroke * 0.42;

    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - step(0.11, viewFacing);
    vec3 coloredInk = mix(vec3(0.020, 0.030, 0.032),
                          surface.rgb * 0.20, 0.30);

    float viewDepth = max(0.0, dot(fragPosition - cameraPosition,
                                   normalize(cameraForward)));
    float foregroundBand = 1.0 - smoothstep(depthSplits.x,
                                             depthSplits.y, viewDepth);
    float backgroundBand = smoothstep(depthSplits.y,
                                      depthSplits.z, viewDepth);
    float detailPresence = 1.0 - backgroundBand * depthStrength * 0.88;
    color = mix(color, coloredInk, edgeInk * 0.62 * detailPresence);

    float accentEdge = accent * smoothstep(0.30, 0.82, authoredValue) *
                       smoothstep(0.18, 0.78, facing);
    color += lightColor * accentEdge * 0.055 * detailPresence;

    if (foregroundReveal > 0.001) {
        float activeCutHeight = mix(revealCutHeight + 8.0,
                                    revealCutHeight, foregroundReveal);
        if (fragPosition.y > activeCutHeight) discard;
        float belowCut = step(fragPosition.y, activeCutHeight);
        float cutBand = (1.0 - smoothstep(0.02, 0.13,
                                          activeCutHeight - fragPosition.y)) *
                        belowCut;
        color = mix(color, coloredInk,
                    cutBand * 0.72 * foregroundReveal);
    }

    float foregroundMass = 1.0 - smoothstep(
        2.5, 8.0, distance(fragPosition.xz, foregroundAnchor.xz));
    float foreground = max(foregroundBand, foregroundMass * 0.55);
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

    float fog = smoothstep(fogNear, fogFar, viewDepth) *
                mix(0.22, 0.42, depthStrength);
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0),
                      surface.a * fragColor.a);
}
