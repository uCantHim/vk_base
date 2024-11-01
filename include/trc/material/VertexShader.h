#pragma once

#include "trc/material/shader/Capability.h"
#include "trc/material/shader/ShaderModuleBuilder.h"
#include "trc/material/shader/ShaderModuleCompiler.h"

namespace trc
{
    namespace code = shader::code;

    namespace VertexCapability
    {
        constexpr shader::Capability kPosition{ "vert_vertexPosition" };
        constexpr shader::Capability kNormal{ "vert_vertexNormal" };
        constexpr shader::Capability kTangent{ "vert_vertexTangent" };
        constexpr shader::Capability kUV{ "vert_vertexUV" };

        constexpr shader::Capability kBoneIndices{ "vert_boneIndices" };
        constexpr shader::Capability kBoneWeights{ "vert_boneWeights" };

        constexpr shader::Capability kModelMatrix{ "vert_modelMatrix" };
        constexpr shader::Capability kViewMatrix{ "vert_viewMatrix" };
        constexpr shader::Capability kProjMatrix{ "vert_projMatrix" };

        constexpr shader::Capability kAnimIndex{ "vert_animIndex" };
        constexpr shader::Capability kAnimKeyframes{ "vert_animKeyframes" };
        constexpr shader::Capability kAnimFrameWeight{ "vert_animFrameWeight" };
        constexpr shader::Capability kAnimMetaBuffer{ "vert_animMetaBuffer" };
        constexpr shader::Capability kAnimDataBuffer{ "vert_animDataBuffer" };
    };

    enum DrawablePushConstIndex : ui32
    {
        eMaterialData,
        eModelMatrix,
        eAnimationData,
    };

    class VertexModule
    {
    public:
        explicit VertexModule(bool animated);

        auto build(const shader::ShaderModule& fragment) && -> shader::ShaderModule;

    private:
        static auto makeVertexCapabilityConfig() -> shader::CapabilityConfig;

        shader::ShaderModuleBuilder builder;

        std::unordered_map<shader::Capability, code::Value> fragmentInputProviders;
    };
} // namespace trc
