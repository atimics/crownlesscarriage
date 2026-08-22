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
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float inkStrength;

out vec4 finalColor;

void main()
{
    int paletteIndex = clamp(int(floor(fragColor.r * 9.0)), 0, 8);
    vec4 paint = characterPalette[paletteIndex];
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(lightDirection);
    vec3 toCamera = normalize(cameraPosition - fragPosition);
    float facing = dot(normal, toLight);
    float wrapped = clamp((facing + 0.32) / 1.32, 0.0, 1.0);
    float lightBand = step(0.48, wrapped);

    bool isSkin = paletteIndex == 0;
    float darkValue = isSkin ? 0.72 : 0.63;
    float lightValue = isSkin ? 1.04 : 1.01;
    vec3 shadowTemperature = isSkin ? vec3(1.04, 0.84, 0.77) :
                                      vec3(0.83, 0.91, 1.02);
    vec3 lightTemperature = vec3(1.035, 1.01, 0.95);
    vec3 temperature = mix(shadowTemperature, lightTemperature, lightBand);
    vec3 color = paint.rgb * mix(darkValue, lightValue, lightBand) *
                 temperature;

    float viewFacing = abs(dot(normal, toCamera));
    float edgeInk = 1.0 - step(0.14, viewFacing);
    vec3 coloredInk = mix(vec3(0.024, 0.030, 0.032),
                          paint.rgb * 0.24, 0.38);
    color = mix(color, coloredInk,
                edgeInk * inkStrength * paletteInk[paletteIndex]);

    color = floor(clamp(color, 0.0, 1.0) * 20.0 + 0.5) / 20.0;
    float distanceToCamera = length(cameraPosition - fragPosition);
    float fog = smoothstep(fogNear, fogFar, distanceToCamera) * 0.24;
    color = mix(color, fogColor, fog);
    finalColor = vec4(clamp(color, 0.0, 1.0), paint.a * colDiffuse.a);
}
