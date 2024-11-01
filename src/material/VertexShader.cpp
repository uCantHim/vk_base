#include "trc/material/VertexShader.h"

#include <initializer_list>
#include <unordered_map>

#include "trc/AssetDescriptor.h"
#include "trc/material/FragmentShader.h"



namespace trc
{

using shader::FunctionType;
using shader::ShaderFunction;
using shader::ShaderModuleCompiler;
using shader::ShaderOutputInterface;

class GlPosition : public ShaderFunction
{
public:
    GlPosition()
        :
        ShaderFunction(
            "calcGlPosition",
            FunctionType{ { vec3{} }, vec4{} }
        )
    {}

    void build(shader::ShaderModuleBuilder& builder, const std::vector<code::Value>& args) override
    {
        auto viewproj = builder.makeMul(
            builder.makeCapabilityAccess(VertexCapability::kProjMatrix),
            builder.makeCapabilityAccess(VertexCapability::kViewMatrix)
        );
        builder.makeReturn(builder.makeMul(
            viewproj,
            builder.makeConstructor<vec4>(args[0], builder.makeConstant(1.0f))
        ));
    }
};

class ApplyAnimation : public ShaderFunction
{
public:
    ApplyAnimation()
        :
        ShaderFunction(
            "applyAnimationTransform",
            FunctionType{ { vec4{} }, vec4{} }
        )
    {}

    void build(shader::ShaderModuleBuilder& builder, const std::vector<code::Value>& args) override
    {
        auto anim = builder.makeCapabilityAccess(VertexCapability::kAnimIndex);
        auto keyframes = builder.makeCapabilityAccess(VertexCapability::kAnimKeyframes);
        auto weight = builder.makeCapabilityAccess(VertexCapability::kAnimFrameWeight);
        auto res = builder.makeExternalCall("applyAnimation", { anim, args[0], keyframes, weight });
        builder.makeReturn(res);
    }
};

class NormalToWorldspace : public ShaderFunction
{
public:
    NormalToWorldspace()
        :
        ShaderFunction("normalToWorldspace", FunctionType{ { vec4{} }, vec3{} })
    {}

    void build(shader::ShaderModuleBuilder& builder, const std::vector<code::Value>& args) override
    {
        auto model = builder.makeCapabilityAccess(VertexCapability::kModelMatrix);
        auto tiModel = builder.makeExternalCall(
            "transpose",
            { builder.makeExternalCall("inverse", {model}) }
        );
        auto normal = builder.makeMemberAccess(builder.makeMul(tiModel, args[0]), "xyz");
        builder.makeReturn(builder.makeExternalCall("normalize", { normal }));
    }
};



VertexModule::VertexModule(bool animated)
{
    auto tbn = [this, animated]() -> code::Value {
        auto zero = builder.makeConstant(0.0f);

        auto normalObjspace = builder.makeCapabilityAccess(VertexCapability::kNormal);
        auto tangentObjspace = builder.makeCapabilityAccess(VertexCapability::kTangent);
        normalObjspace = builder.makeConstructor<vec4>(normalObjspace, zero);
        tangentObjspace = builder.makeConstructor<vec4>(tangentObjspace, zero);
        if (animated) {
            normalObjspace = builder.makeCall<ApplyAnimation>({ normalObjspace });
            tangentObjspace = builder.makeCall<ApplyAnimation>({ tangentObjspace });
        }

        auto normal = builder.makeCall<NormalToWorldspace>({ normalObjspace });
        auto tangent = builder.makeCall<NormalToWorldspace>({ tangentObjspace });
        auto bitangent = builder.makeExternalCall("cross", { normal, tangent });

        auto tbn = builder.makeConstructor<mat3>(tangent, bitangent, normal);

        return tbn;
    }();

    fragmentInputProviders = {
        {
            MaterialCapability::kVertexWorldPos,
            [this, animated]() -> code::Value
            {
                auto objPos = builder.makeCapabilityAccess(VertexCapability::kPosition);
                auto modelMat = builder.makeCapabilityAccess(VertexCapability::kModelMatrix);
                auto objPos4 = builder.makeConstructor<vec4>(objPos, builder.makeConstant(1.0f));
                if (animated)
                {
                    builder.includeCode("material_utils/animation.glsl", {
                        { "animationMetaDataDescriptorName", VertexCapability::kAnimMetaBuffer },
                        { "animationDataDescriptorName", VertexCapability::kAnimDataBuffer },
                        { "vertexBoneIndicesAttribName", VertexCapability::kBoneIndices },
                        { "vertexBoneWeightsAttribName", VertexCapability::kBoneWeights },
                    });
                    objPos4 = builder.makeCall<ApplyAnimation>({ objPos4 });
                }

                auto worldPos = builder.makeMul(modelMat, objPos4);
                return builder.makeMemberAccess(worldPos, "xyz");
            }()
        },
        { MaterialCapability::kTangentToWorldSpaceMatrix, tbn },
        { MaterialCapability::kVertexUV, builder.makeCapabilityAccess(VertexCapability::kUV) },
        { MaterialCapability::kVertexNormal, tbn },
    };
}

auto VertexModule::build(const shader::ShaderModule& fragment) && -> shader::ShaderModule
{
    ShaderOutputInterface shaderOutput;
    for (const auto& out : fragment.getRequiredShaderInputs())
    {
        auto loc = builder.makeOutputLocation(out.location, out.type);

        try {
            auto inputNode = fragmentInputProviders.at(out.capability);
            shaderOutput.makeStore(loc, inputNode);
        }
        catch (const std::out_of_range&)
        {
            log::warn << "Warning: [In VertexShaderBuilder::buildVertexShader]: Fragment"
                      << " capability \"" << out.capability.toString()
                      << "\" is requested as an output but not implemented.\n";
        }
    }

    // Always add the gl_Position output
    shaderOutput.makeStore(
        builder.makeExternalIdentifier("gl_Position"),
        builder.makeCall<GlPosition>(
            { fragmentInputProviders.at(MaterialCapability::kVertexWorldPos) }
        )
    );

    return ShaderModuleCompiler{}.compile(
        shaderOutput,
        std::move(builder),
        makeVertexCapabilityConfig()
    );
}

auto VertexModule::makeVertexCapabilityConfig() -> shader::CapabilityConfig
{
    using shader::CapabilityConfig;

    static auto config = []{
        CapabilityConfig config;
        auto& code = config.getCodeBuilder();

        config.addGlobalShaderExtension("GL_GOOGLE_include_directive");

        auto cameraMatrices = config.addResource(CapabilityConfig::DescriptorBinding{
            .setName="global_data",
            .bindingIndex=0,
            .descriptorType="uniform",
            .descriptorName="camera",
            .layoutQualifier="std140",
            .descriptorContent=
                "mat4 viewMatrix;\n"
                "mat4 projMatrix;\n"
                "mat4 inverseViewMatrix;\n"
                "mat4 inverseProjMatrix;\n"
        });

        auto modelPc = config.addResource(CapabilityConfig::PushConstant{
            mat4{}, DrawablePushConstIndex::eModelMatrix
        });
        auto animDataPc = config.addResource(CapabilityConfig::PushConstant{
            code.makeStructType("AnimationPushConstantData", {
                { uint{}, "animation" },
                { uvec2{}, "keyframes" },
                { float{}, "keyframeWeigth" },
            }),
            DrawablePushConstIndex::eAnimationData
        });
        config.addShaderInclude(animDataPc, util::Pathlet("material_utils/animation_data.glsl"));

        auto animMeta = config.addResource(CapabilityConfig::DescriptorBinding{
            .setName="asset_registry",
            .bindingIndex=AssetDescriptor::getBindingIndex(AssetDescriptorBinding::eAnimationMetadata),
            .descriptorType="restrict readonly buffer",
            .descriptorName="AnimationMetaDataDescriptor",
            .layoutQualifier="std430",
            .descriptorContent="AnimationMetaData metas[];"
        });
        auto animBuffer = config.addResource(CapabilityConfig::DescriptorBinding{
            .setName="asset_registry",
            .bindingIndex=AssetDescriptor::getBindingIndex(AssetDescriptorBinding::eAnimationData),
            .descriptorType="restrict readonly buffer",
            .descriptorName="AnimationDataDescriptor",
            .layoutQualifier="std140",
            .descriptorContent="mat4 boneMatrices[];"
        });
        config.addShaderInclude(animMeta, util::Pathlet("material_utils/animation_data.glsl"));
        config.linkCapability(VertexCapability::kAnimMetaBuffer, animMeta);
        config.linkCapability(VertexCapability::kAnimDataBuffer, animBuffer);

        auto vPos     = config.addResource(CapabilityConfig::ShaderInput{ vec3{}, 0 });
        auto vNormal  = config.addResource(CapabilityConfig::ShaderInput{ vec3{}, 1 });
        auto vUV      = config.addResource(CapabilityConfig::ShaderInput{ vec2{}, 2 });
        auto vTangent = config.addResource(CapabilityConfig::ShaderInput{ vec3{}, 3 });
        auto vBoneIndices = config.addResource(CapabilityConfig::ShaderInput{ uvec4{}, 4 });
        auto vBoneWeights = config.addResource(CapabilityConfig::ShaderInput{ vec4{}, 5 });

        config.linkCapability(VertexCapability::kPosition, vPos);
        config.linkCapability(VertexCapability::kNormal, vNormal);
        config.linkCapability(VertexCapability::kTangent, vTangent);
        config.linkCapability(VertexCapability::kUV, vUV);
        config.linkCapability(VertexCapability::kBoneIndices, vBoneIndices);
        config.linkCapability(VertexCapability::kBoneWeights, vBoneWeights);

        // Model matrix
        config.linkCapability(VertexCapability::kModelMatrix, modelPc);

        // Camera matrices
        auto camera = config.accessResource(cameraMatrices);
        config.linkCapability(VertexCapability::kViewMatrix,
                              code.makeMemberAccess(camera, "viewMatrix"),
                              { cameraMatrices });
        config.linkCapability(VertexCapability::kProjMatrix,
                              code.makeMemberAccess(camera, "projMatrix"),
                              { cameraMatrices });

        // Animation data
        auto animData = config.accessResource(animDataPc);
        config.linkCapability(VertexCapability::kAnimIndex,
                              code.makeMemberAccess(animData, "animation"),
                              { animDataPc });
        config.linkCapability(VertexCapability::kAnimKeyframes,
                              code.makeMemberAccess(animData, "keyframes"),
                              { animDataPc });
        config.linkCapability(VertexCapability::kAnimFrameWeight,
                              code.makeMemberAccess(animData, "keyframeWeigth"),
                              { animDataPc });

        return config;
    }();

    return config;
}

} // namespace trc
