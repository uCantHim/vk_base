// Animation related stuff

#ifndef BONE_INDICES_INPUT_LOCATION
#define BONE_INDICES_INPUT_LOCATION 4
#endif

#ifndef BONE_WEIGHTS_INPUT_LOCATION
#define BONE_WEIGHTS_INPUT_LOCATION 5
#endif

#ifndef ANIM_DESCRIPTOR_SET_BINDING
#define ANIM_DESCRIPTOR_SET_BINDING 3
#endif

struct AnimationMetaData
{
    uint offset;
    uint frameCount;
};

layout (location = BONE_INDICES_INPUT_LOCATION) in uvec4 vertexBoneIndices;
layout (location = BONE_WEIGHTS_INPUT_LOCATION) in vec4 vertexBoneWeights;

layout (set = ANIM_DESCRIPTOR_SET_BINDING, binding = 0, std140) restrict readonly buffer
AnimationMeta
{
    // Indexed by animation indices provided by the client
    AnimationMetaData metas[];
} animMeta;

layout (set = ANIM_DESCRIPTOR_SET_BINDING, binding = 1, std140) restrict readonly buffer
Animation
{
    // Animations start at offsets defined in the AnimationMetaData array
    mat4 boneMatrices[];
} anim;


void applyAnimation(uint animIndex, vec4 vertPos, uint frames[2], float frameWeight)
{
    vec4 currentFramePos = vec4(0.0);
    vec4 nextFramePos = vec4(0.0);

    const uint baseOffset = animMeta.metas[animIndex].offset;
    const uint frameCount = animMeta.metas[animIndex].frameCount;

    for (int i = 0; i < 4; i++)
    {
        float weight = vertexBoneWeights[i];
        if (weight <= 0.0) {
            continue;
        }

        uint boneIndex = vertexBoneIndices[i];
        uint currentFrameOffset = baseOffset + frameCount * frames[0] + boneIndex;
        uint nextFrameOffset = baseOffset + frameCount * frames[1] + boneIndex;

        mat4 currentBoneMatrix = anim.boneMatrices[currentFrameOffset];
        mat4 nextBoneMatrix = anim.boneMatrices[nextFrameOffset];

        currentFramePos += currentBoneMatrix * vertPos;
        nextFramePos += nextBoneMatrix * vertPos;
    }

    vertPos = currentFramePos * (1.0 - frameWeight) + nextFramePos * frameWeight;
}
