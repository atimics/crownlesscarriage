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
uniform float vertexColorsAreAlbedo;
uniform vec3 characterRimTint;
uniform float characterRimStrength;

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
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse;
    albedo *= mix(vec4(1.0), fragColor, vertexColorsAreAlbedo);
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    vec3 toCamera = normalize(cameraPosition - fragPosition);
    float facing = dot(normal, toLight);

    float wrapped = clamp((facing + 0.30) / 1.30, 0.0, 1.0);
    float lightBand = step(0.47, wrapped);
    float shade = mix(0.70, 1.03, lightBand);
    float authoredPaint = (1.0 - step(0.98, fragColor.r)) *
                          (1.0 - vertexColorsAreAlbedo);
    float authoredValue = fragColor.g < 0.375 ? 0.68 :
                          fragColor.g < 0.625 ? 0.86 : 1.05;
    int surface = int(floor(fragColor.a * 8.0));
    float materialStyle = float(surface < 6) * (1.0 - vertexColorsAreAlbedo);
    vec4 response = surfaceResponse(surface);
    float skinMask = smoothstep(0.40, 0.52, albedo.r) *
                     (1.0 - smoothstep(0.88, 0.96, albedo.r)) *
                     smoothstep(0.22, 0.32, albedo.g) *
                     (1.0 - smoothstep(0.78, 0.88, albedo.g)) *
                     smoothstep(0.14, 0.22, albedo.b) *
                     (1.0 - smoothstep(0.72, 0.82, albedo.b));
    skinMask = mix(skinMask, float(surface == 0), materialStyle);
    shade = mix(shade, mix(0.76, 1.05, lightBand), skinMask);
    shade = mix(shade, mix(response.x, response.y, lightBand), materialStyle);
    shade = mix(shade, authoredValue, authoredPaint *
                mix(0.78, surface == 4 ? 0.28 : 0.58, materialStyle));
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

    float skyExposure = smoothstep(-0.30, 0.82, normal.y);
    color += albedo.rgb * vec3(0.74, 0.91, 1.04) * skyExposure *
             (1.0 - lightBand) * 0.095;

    vec3 halfDirection = normalize(toCamera + toLight);
    float gloss = pow(max(dot(normal, halfDirection), 0.0),
                       mix(18.0, response.z, materialStyle));
    float glossWidth = max(fwidth(gloss), 0.055);
    float paintedHighlight = mix(step(0.62, gloss),
        smoothstep(0.52 - glossWidth, 0.52 + glossWidth, gloss), materialStyle);
    vec3 highlightColor = surface == 4 ? mix(vec3(1.0), albedo.rgb, 0.30) :
                                          vec3(1.0, 0.91, 0.76);
    color += highlightColor * paintedHighlight * lightBand *
             mix(0.038, response.w, materialStyle);

    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - step(0.105, viewFacing);
    vec3 ink = mix(vec3(0.025, 0.032, 0.034), albedo.rgb * 0.22, 0.34);
    color = mix(color, ink, edgeInk * inkStrength);
    float litEdge = edgeInk * smoothstep(0.08, 0.68, facing) * lightBand;
    color += vec3(0.19, 0.28, 0.28) * litEdge * 0.055;

    float silhouette = smoothstep(0.70, 0.94, 1.0 - viewFacing);
    float heroRim = silhouette * (0.065 + skyExposure * 0.105) *
                    characterRimStrength;
    vec3 heroRimColor = mix(characterRimTint,
                            vec3(0.70, 0.52, 0.24), lightBand * 0.32);
    color += heroRimColor * heroRim;

    float distanceToCamera = length(cameraPosition - fragPosition);
    float fog = smoothstep(fogNear, fogFar, distanceToCamera) * 0.24;
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
