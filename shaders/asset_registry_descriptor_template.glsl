// Descriptor binding declarations for the asset descriptor

#include "material.glsl"
#include "vertex.glsl"

struct AnimationMetaData
{
    uint baseOffset;
    uint frameCount;
    uint boneCount;
};

#define TEX_BINDING $texture_descriptor_binding
#define FONT_BINDING $font_descriptor_binding
#define GEO_VERT_BINDING $geometry_vertex_descriptor_binding
#define GEO_INDEX_BINDING $geometry_index_descriptor_binding
#define ANIM_META_BINDING $animation_meta_descriptor_binding
#define ANIM_DATA_BINDING $animation_data_descriptor_binding

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = TEX_BINDING)
    uniform sampler2D textures[];

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = FONT_BINDING)
    uniform sampler2D glyphTextures[];

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = GEO_VERT_BINDING, std430)
    restrict readonly buffer VertexBuffers
{
    Vertex vertices[];
} vertexBuffers[];

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = GEO_INDEX_BINDING, std430)
    restrict readonly buffer IndexBuffers
{
    uint indices[];
} indexBuffers[];

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = ANIM_META_BINDING, std430)
    restrict readonly buffer AnimationMeta
{
    // Indexed by animation indices provided by the client
    AnimationMetaData metas[];
} animMeta;

layout (set = ASSET_DESCRIPTOR_SET_BINDING, binding = ANIM_DATA_BINDING, std140)
    restrict readonly buffer Animation
{
    // Animations start at offsets defined in the AnimationMetaData array
    mat4 boneMatrices[];
} animations;
