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
uniform vec3 depthSplits;
uniform float depthStrength;

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

float hash31(vec3 point)
{
    point = fract(point * 0.1031);
    point += dot(point, point.yzx + 33.33);
    return fract((point.x + point.y) * point.z);
}

vec2 foliagePlane(vec3 point, vec3 normal)
{
    vec3 facing = abs(normal);
    if (facing.y >= facing.x && facing.y >= facing.z) return point.xz;
    if (facing.x >= facing.z) return point.zy;
    return point.xy;
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    float facing = dot(normal, toLight);
    float viewDepth = max(0.0, dot(fragPosition - cameraPosition,
                                   normalize(cameraForward)));
    float backgroundBand = smoothstep(depthSplits.y,
                                      depthSplits.z, viewDepth);
    float detailPresence = 1.0 - backgroundBand * depthStrength * 0.92;

    /* Keep each polygon as one clear painted value. The crown mesh supplies
       flat normals, so these bands never turn into curved rubber highlights. */
    float diffuse = facing < 0.08 ? 0.02 :
                    facing < 0.58 ? 0.24 : 0.50;
    float sky = normal.y < -0.24 ? 0.66 :
                normal.y < 0.48 ? 0.80 : 0.96;
    vec3 light = ambientColor * mix(0.72, 0.98, sky);
    light += lightColor * diffuse;
    light += vec3(0.06, 0.15, 0.13) * max(normal.y, 0.0) * 0.08;
    light += vec3(0.12, 0.07, 0.035) * max(-normal.y, 0.0) * 0.05;

    /* Hard world-space chips suggest leaf clumps without a texture, UV seam,
       or animated noise. Their size is broad enough to survive the art grid. */
    vec2 plane = foliagePlane(fragPosition, normal);
    float faceSeed = hash31(floor(normal * 17.0 + vec3(23.0)));
    vec2 chipPoint = plane * 3.85 + vec2(faceSeed * 13.0,
                                         faceSeed * 29.0);
    vec2 chipCenter = fract(chipPoint) - 0.5;
    float chipShape = 1.0 - step(0.72,
                                 abs(chipCenter.x) +
                                 abs(chipCenter.y) * 0.82);
    float chipValue = hash21(floor(chipPoint) + vec2(faceSeed * 7.0));
    float shadowChip = (1.0 - step(0.30, chipValue)) * chipShape;
    float lightChip = step(0.72, chipValue) * chipShape *
                      step(-0.08, normal.y);

    vec3 paintTemperature = mix(shadowColor, vec3(1.0),
                                smoothstep(-0.08, 0.58, facing));
    vec3 color = albedo.rgb * light * paintTemperature;
    color *= mix(vec3(1.0), vec3(0.63, 0.82, 0.69),
                 shadowChip * 0.62 * detailPresence);
    vec3 warmLeaf = color * vec3(1.28, 1.18, 0.78) +
                    vec3(0.035, 0.026, 0.004);
    color = mix(color, warmLeaf, lightChip * 0.56 * detailPresence);

    /* Foliage has no glossy specular. A small cool rim and restrained ink
       retain the crown silhouette without outlining every mass in black. */
    vec3 viewDirection = normalize(cameraPosition - fragPosition);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);
    color += vec3(0.10, 0.23, 0.18) * rim * 0.025 * detailPresence;
    float viewFacing = abs(dot(normal, viewDirection));
    float edgeInk = 1.0 - step(0.10, viewFacing);
    color = mix(color, albedo.rgb * 0.24,
                edgeInk * 0.42 * detailPresence);

    /* Distant crowns become one quiet silhouette group. Chip calligraphy is
       reserved for the foreground where it survives the fixed pixel grid. */
    float backgroundWeight = backgroundBand * depthStrength;
    float luminance = perceivedGray(color);
    vec3 quietCrown = mix(vec3(luminance) * vec3(0.80, 0.96, 1.02),
                          fogColor + vec3(0.040, 0.050, 0.045), 0.35);
    color = mix(color, quietCrown, backgroundWeight * 0.56);
    float fog = smoothstep(fogNear, fogFar, viewDepth) *
                mix(0.24, 0.44, depthStrength);
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
