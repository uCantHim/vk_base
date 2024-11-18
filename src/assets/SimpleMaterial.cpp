#include "trc/assets/SimpleMaterial.h"

#include "trc/assets/import/InternalFormat.h"
#include "trc/material/FragmentShader.h"
#include "trc/material/ShaderFunctions.h"
#include "trc/material/TorchMaterialSettings.h"
#include "trc/material/VertexShader.h"



namespace trc
{

void SimpleMaterialData::resolveReferences(AssetManager& man)
{
    if (!albedoTexture.empty()) {
        albedoTexture.resolve(man);
    }
    if (!normalTexture.empty()) {
        normalTexture.resolve(man);
    }
}

void SimpleMaterialData::serialize(std::ostream& os) const
{
    serial::SimpleMaterial mat = internal::serializeAssetData(*this);
    mat.SerializeToOstream(&os);
}

void SimpleMaterialData::deserialize(std::istream& is)
{
    serial::SimpleMaterial mat;
    mat.ParseFromIstream(&is);
    *this = internal::deserializeAssetData(mat);
}



auto makeMaterial(const SimpleMaterialData& data) -> MaterialData
{
    constexpr auto kCapCurrentMat = "simplemat_param_obj";

    shader::ShaderModuleBuilder builder;
    auto capabilities = makeFragmentCapabilityConfig();

    // Declare the material data struct as a push constant
    auto pcStructType = builder.makeStructType(
        "MaterialParameters",
        {
            { vec3{},  "color" },
            { float{}, "specularFactor" },
            { float{}, "roughness" },
            { float{}, "metallicness" },
            { uint{},  "albedoTexture" },
            { uint{},  "normalTexture" },
            { bool{},  "emissive" },
        }
    );
    auto pc = capabilities.addResource(shader::CapabilityConfig::PushConstant{
        pcStructType,
        DrawablePushConstIndex::eMaterialData,
    });
    capabilities.linkCapability(kCapCurrentMat, pc);

    auto mat = builder.makeCapabilityAccess(kCapCurrentMat);
    auto colorParam        = builder.makeMemberAccess(mat, "color");
    auto specularParam     = builder.makeMemberAccess(mat, "specularFactor");
    auto roughnessParam    = builder.makeMemberAccess(mat, "roughness");
    auto metallicnessParam = builder.makeMemberAccess(mat, "metallicness");
    auto emissiveParam     = builder.makeMemberAccess(mat, "emissive");

    // TODO: This remains the old, non-dynamic way until the buffer is implemented.
    //colorParam        = builder.makeConstant(data.color);
    //specularParam     = builder.makeConstant(data.specularCoefficient);
    //roughnessParam    = builder.makeConstant(data.roughness);
    //metallicnessParam = builder.makeConstant(data.metallicness);
    //emissiveParam     = builder.makeConstant(data.emissive);

    // Build output values
    code::Value opacity = builder.makeConstant(data.opacity);
    code::Value color = builder.makeConstructor<vec4>(colorParam, opacity);
    if (!data.albedoTexture.empty())
    {
        color = builder.makeCall<TextureSample>({
            builder.makeSpecializationConstant(
                std::make_shared<RuntimeTextureIndex>(data.albedoTexture)
            ),
            builder.makeCapabilityAccess(MaterialCapability::kVertexUV)
        });
    }

    code::Value normal = builder.makeCapabilityAccess(MaterialCapability::kVertexNormal);;
    if (!data.normalTexture.empty())
    {
        auto sampledNormal = builder.makeCall<TextureSample>({
            builder.makeSpecializationConstant(
                std::make_shared<RuntimeTextureIndex>(data.normalTexture)
            ),
            builder.makeCapabilityAccess(MaterialCapability::kVertexUV)
        });
        normal = builder.makeCall<TangentToWorldspace>({
            builder.makeMemberAccess(sampledNormal, "rgb")
        });
    }

    FragmentModule frag;
    frag.setParameter(FragmentModule::Parameter::eColor,          color);
    frag.setParameter(FragmentModule::Parameter::eNormal,         normal);
    frag.setParameter(FragmentModule::Parameter::eSpecularFactor, specularParam);
    frag.setParameter(FragmentModule::Parameter::eRoughness,      roughnessParam);
    frag.setParameter(FragmentModule::Parameter::eMetallicness,   metallicnessParam);
    frag.setParameter(FragmentModule::Parameter::eEmissive,
                      builder.makeCast<float>(emissiveParam));

    const bool transparent = data.opacity < 1.0f;
    auto res = MaterialData{ MaterialBaseInfo{
        frag.build(std::move(builder), transparent, capabilities),
        transparent
    }};

    struct PcData
    {
        vec3 color;
        float specularFactor;
        float roughness;
        float metallicness;
        ui32 albedoTexture;
        ui32 normalTexture;
        bool emissive;
    };

    PcData pcData{
        data.color,
        data.specularCoefficient,
        data.roughness,
        data.metallicness,
        std::numeric_limits<ui32>::max(),
        std::numeric_limits<ui32>::max(),
        data.emissive
    };
    res.runtimeValueDefaults.emplace_back(
        DrawablePushConstIndex::eMaterialData,
        std::vector<std::byte>{
            reinterpret_cast<const std::byte*>(&pcData),
            reinterpret_cast<const std::byte*>(&pcData) + sizeof(PcData)
        }
    );

    return res;
}

} // namespace trc
