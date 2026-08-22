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
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;

out vec4 finalColor;

float hash21(vec2 point)
{
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 normal = normalize(fragNormal);
    float facing = dot(normal, normalize(lightDirection));
    float key = max(facing, 0.0);
    float wrap = clamp((facing + 0.32) / 1.32, 0.0, 1.0);
    float hemisphere = normal.y * 0.5 + 0.5;

    /* Preserve form on camera-facing walls. At the final art-pixel scale,
       letting the cool fill collapse any further turns roofs and facades into
       the same near-black mass. */
    vec3 light = ambientColor * mix(0.82, 1.05, hemisphere);
    float diffuse = key * 0.52 + wrap * 0.16;
    /* Three broad values survive the low-resolution composite better than a
       smooth miniature render and keep buildings, bridge stone, and props in
       the same stepped language as the cast. */
    diffuse = floor(diffuse * 2.0 + 0.5) / 2.0;
    light += lightColor * diffuse;

    vec3 viewDirection = normalize(cameraPosition - fragPosition);
    vec3 halfDirection = normalize(viewDirection + normalize(lightDirection));
    float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0) * 0.10;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);

    /* Low-frequency value breakup keeps large procedural surfaces from
       reading as single-color slabs without introducing noisy textures. */
    float variation = hash21(floor(fragPosition.xz * 0.48)) - 0.5;
    vec3 color = albedo.rgb * light * (1.0 + variation * 0.014);
    color += lightColor * specular;
    color += vec3(0.20, 0.46, 0.48) * rim * 0.045;

    /* A view-stable colored edge keeps large structural silhouettes legible
       without a screen-space outline pass that would crawl between pixels. */
    float viewFacing = abs(dot(normal, viewDirection));
    float edgeInk = 1.0 - step(0.13, viewFacing);
    vec3 coloredInk = mix(vec3(0.020, 0.031, 0.034),
                          albedo.rgb * 0.20, 0.28);
    color = mix(color, coloredInk, edgeInk * 0.72);
    color = floor(clamp(color, 0.0, 1.0) * 18.0 + 0.5) / 18.0;

    float distanceToCamera = length(cameraPosition - fragPosition);
    float fog = smoothstep(fogNear, fogFar, distanceToCamera) * 0.34;
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), albedo.a);
}
