#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;

out vec4 finalColor;

float hash21(vec2 point)
{
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
}

void main()
{
    vec4 source = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 color = source.rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    /* Restrained pigments, warmer light, and cool shadows make code-drawn
       geometry and authored glTF materials belong to one handcrafted world. */
    color = mix(vec3(luminance), color, 0.90);
    color *= vec3(1.035, 1.005, 0.965);
    color = (color - 0.5) * 1.075 + 0.5;
    color *= 0.88;

    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float vignette = 1.0 - smoothstep(0.38, 1.28,
                                     dot(centered, centered));
    color *= mix(0.78, 1.03, vignette);

    float grain = hash21(fragTexCoord * resolution) - 0.5;
    color += grain * 0.010;
    finalColor = vec4(clamp(color, 0.0, 1.0), source.a);
}
