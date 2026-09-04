#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[32];

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
    int bone0 = int(vertexBoneIndices.x);
    int bone1 = int(vertexBoneIndices.y);
    int bone2 = int(vertexBoneIndices.z);
    int bone3 = int(vertexBoneIndices.w);
    vec4 localPosition =
        vertexBoneWeights.x * (boneMatrices[bone0] * vec4(vertexPosition, 1.0)) +
        vertexBoneWeights.y * (boneMatrices[bone1] * vec4(vertexPosition, 1.0)) +
        vertexBoneWeights.z * (boneMatrices[bone2] * vec4(vertexPosition, 1.0)) +
        vertexBoneWeights.w * (boneMatrices[bone3] * vec4(vertexPosition, 1.0));
    vec4 localNormal =
        vertexBoneWeights.x * (boneMatrices[bone0] * vec4(vertexNormal, 0.0)) +
        vertexBoneWeights.y * (boneMatrices[bone1] * vec4(vertexNormal, 0.0)) +
        vertexBoneWeights.z * (boneMatrices[bone2] * vec4(vertexNormal, 0.0)) +
        vertexBoneWeights.w * (boneMatrices[bone3] * vec4(vertexNormal, 0.0));
    vec4 worldPosition = matModel * localPosition;
    fragPosition = worldPosition.xyz;
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize((matNormal * vec4(localNormal.xyz, 0.0)).xyz);
    fragColor = vertexColor;
    gl_Position = mvp * localPosition;
}
