#version 460
#extension GL_GOOGLE_include_directive : require

#define BONE_INDICES_INPUT_LOCATION 4
#define BONE_WEIGHTS_INPUT_LOCATION 5
#define ASSET_DESCRIPTOR_SET_BINDING 1
#include "animation.glsl"

layout (location = 0) in vec3 vertexPosition;

layout (set = 0, binding = 0, std430) buffer ShadowMatrices
{
    // These are the view-proj matrices
    mat4 shadowMatrices[];
};

layout (push_constant) uniform PushConstants
{
    mat4 modelMatrix;
    uint shadowIndex;  // Index into shadow matrix buffer

    AnimationPushConstantData animData;
};

void main()
{
    mat4 viewProj = shadowMatrices[shadowIndex];
    vec4 vertPos = vec4(vertexPosition, 1.0);
    vertPos = applyAnimation(animData.animation, vertPos, animData.keyframes, animData.keyframeWeigth);
    vertPos.w = 1.0;

    gl_Position = viewProj * modelMatrix * vertPos;
}
