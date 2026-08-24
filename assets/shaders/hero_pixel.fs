#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 shadowColor;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float inkStrength;

out vec4 finalColor;

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse;
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    vec3 toCamera = normalize(cameraPosition - fragPosition);
    float facing = dot(normal, toLight);

    /* The figure gets exactly two broad paint values. The decision is stable
       in object space; the whole-scene nearest upscale supplies the pixels. */
    float wrapped = clamp((facing + 0.30) / 1.30, 0.0, 1.0);
    float lightBand = step(0.47, wrapped);
    float shade = mix(0.70, 1.03, lightBand);
    float authoredPaint = 1.0 - step(0.98, fragColor.r);
    float authoredValue = fragColor.g < 0.375 ? 0.68 :
                          fragColor.g < 0.625 ? 0.86 : 1.05;
    float skinMask = smoothstep(0.40, 0.52, albedo.r) *
                     (1.0 - smoothstep(0.88, 0.96, albedo.r)) *
                     smoothstep(0.22, 0.32, albedo.g) *
                     (1.0 - smoothstep(0.78, 0.88, albedo.g)) *
                     smoothstep(0.14, 0.22, albedo.b) *
                     (1.0 - smoothstep(0.72, 0.82, albedo.b));
    shade = mix(shade, mix(0.76, 1.05, lightBand), skinMask);
    shade = mix(shade, authoredValue, authoredPaint * 0.78);
    vec3 bodyShadow = vec3(0.83, 0.91, 1.02);
    vec3 skinShadow = vec3(1.04, 0.83, 0.76);
    vec3 shadowTemperature = mix(bodyShadow, skinShadow, skinMask) *
                             shadowColor;
    vec3 temperature = mix(shadowTemperature,
                           vec3(1.035, 1.01, 0.95), lightBand);
    vec3 color = albedo.rgb * shade * temperature;
    float foldStrength = fragColor.b * authoredPaint;
    float foldShadow = (1.0 - smoothstep(-0.18, 0.48, facing)) *
                       foldStrength;
    color *= 1.0 - foldShadow * 0.11;

    /* A small sky fill keeps the shadow side of the crown, hands, and cape
       readable against dark architecture while retaining the two-value paint
       treatment. It is normal-driven, so it never creates a second pixel
       grid or an unstable screen-space halo. */
    float skyExposure = smoothstep(-0.30, 0.82, normal.y);
    color += albedo.rgb * vec3(0.74, 0.91, 1.04) * skyExposure *
             (1.0 - lightBand) * 0.072;

    vec3 halfDirection = normalize(toCamera + toLight);
    float paintedHighlight = pow(max(dot(normal, halfDirection), 0.0), 18.0);
    paintedHighlight = step(0.62, paintedHighlight) * lightBand;
    color += vec3(1.0, 0.91, 0.76) * paintedHighlight * 0.038;

    /* Edge-facing fragments become colored ink. This is deliberately inside
       the silhouette, so it remains stable on the coarse world target and
       does not grow or crawl as an inverted-hull outline would. */
    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - step(0.105, viewFacing);
    vec3 ink = mix(vec3(0.025, 0.032, 0.034), albedo.rgb * 0.22, 0.34);
    color = mix(color, ink, edgeInk * inkStrength);
    float litEdge = edgeInk * smoothstep(0.08, 0.68, facing) * lightBand;
    color += vec3(0.19, 0.28, 0.28) * litEdge * 0.055;

    float distanceToCamera = length(cameraPosition - fragPosition);
    float fog = smoothstep(fogNear, fogFar, distanceToCamera) * 0.24;
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
