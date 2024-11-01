#include "trc/material/FragmentShader.h"

#include "trc/material/shader/ShaderOutputInterface.h"



namespace trc
{

auto to_string(FragmentModule::Parameter param) -> std::string
{
    switch (param)
    {
    case FragmentModule::Parameter::eColor: return "Color";
    case FragmentModule::Parameter::eNormal: return "Normal";
    case FragmentModule::Parameter::eSpecularFactor: return "Specular Coefficient";
    case FragmentModule::Parameter::eRoughness: return "Roughness";
    case FragmentModule::Parameter::eMetallicness: return "Metallicness";
    case FragmentModule::Parameter::eEmissive: return "Emissive";
    };

    throw std::logic_error("Enum `FragmentModule::Parameter` exhausted.");
}



void FragmentModule::setParameter(Parameter param, code::Value value)
{
    parameters[static_cast<size_t>(param)] = value;
}

auto FragmentModule::build(
    shader::ShaderModuleBuilder builder,
    bool transparent,
    const shader::CapabilityConfig& capabilityConfig) -> shader::ShaderModule
{
    shader::ShaderOutputInterface output;

    // Ensure that every required parameter exists and has a value
    fillDefaultValues(builder);

    // Cast the emissive value (bool) to float.
    auto emissiveParam = *parameters[static_cast<size_t>(Parameter::eEmissive)];
    setParameter(Parameter::eEmissive, builder.makeCast<float>(emissiveParam));

    if (!transparent)
    {
        static constexpr std::array<std::string_view, FragmentModule::kNumParams> paramAccessors{{
            "", "", "x", "y", "z", "w",
        }};

        auto storeOutput = [&, this](Parameter param, code::Value out) {
            const auto index = static_cast<size_t>(param);
            if (!paramAccessors[index].empty()) {
                out = builder.makeMemberAccess(out, std::string(paramAccessors[index]));
            }
            output.makeStore(out, getParamValue(param));
        };

        auto outNormal = builder.makeOutputLocation(0, vec3{});
        auto outAlbedo = builder.makeOutputLocation(1, vec4{});
        auto outMaterial = builder.makeOutputLocation(2, vec4{});

        storeOutput(Parameter::eColor, outAlbedo);
        storeOutput(Parameter::eNormal, outNormal);
        storeOutput(Parameter::eSpecularFactor, outMaterial);
        storeOutput(Parameter::eMetallicness, outMaterial);
        storeOutput(Parameter::eRoughness, outMaterial);
        storeOutput(Parameter::eEmissive, outMaterial);
    }
    else {
        builder.includeCode("material_utils/append_fragment.glsl", {
            { "nextFragmentListIndex",   FragmentCapability::kNextFragmentListIndex },
            { "maxFragmentListIndex",    FragmentCapability::kMaxFragmentListIndex },
            { "fragmentListHeadPointer", FragmentCapability::kFragmentListHeadPointerImage },
            { "fragmentList",            FragmentCapability::kFragmentListBuffer },
        });
        builder.includeCode("material_utils/shadow.glsl", {
            { "shadowMatrixBufferName", FragmentCapability::kShadowMatrices },
        });
        builder.includeCode("material_utils/lighting.glsl", {
            { "lightBufferName", FragmentCapability::kLightBuffer },
        });

        auto color = getParamValue(Parameter::eColor);
        builder.annotateType(color, vec4{});

        auto alpha = builder.makeMemberAccess(color, "a");
        auto doLighting = builder.makeNot(builder.makeCast<bool>(getParamValue(Parameter::eEmissive)));
        auto isVisible = builder.makeGreaterThan(alpha, builder.makeConstant(0.0f));
        auto cond = builder.makeAnd(isVisible, doLighting);

        color = builder.makeConditional(
            cond,
            // if true:
            builder.makeConstructor<vec4>(
                builder.makeExternalCall("calcLighting", {
                    builder.makeMemberAccess(color, "xyz"),
                    builder.makeCapabilityAccess(MaterialCapability::kVertexWorldPos),
                    getParamValue(Parameter::eNormal),
                    builder.makeCapabilityAccess(MaterialCapability::kCameraWorldPos),
                    builder.makeExternalCall("MaterialParams", {
                        getParamValue(Parameter::eSpecularFactor),
                        getParamValue(Parameter::eRoughness),
                        getParamValue(Parameter::eMetallicness),
                    })
                }),
                builder.makeMemberAccess(color, "a")
            ),
            // if false:
            color
        );

        // TODO: Ideally, we only call this if `isVisible` evaluates to true,
        // though we don't have a mechanism for conditional output yet.
        output.makeBuiltinCall("appendFragment", { color });
    }

    builder.enableEarlyFragmentTest();

    return shader::ShaderModuleCompiler{}.compile(
        output,
        std::move(builder),
        capabilityConfig
    );
}

auto FragmentModule::buildClosesthitShader(shader::ShaderModuleBuilder builder)
    -> shader::ShaderModule
{
    namespace cap = RayHitCapability;
    shader::ShaderOutputInterface out;

    fillDefaultValues(builder);
    out.makeStore(
        builder.makeCapabilityAccess(cap::kOutColor),
        builder.makeConditional(
            getParamValue(Parameter::eEmissive),
            // Use albedo if the material is emissive
            getParamValue(Parameter::eColor),
            // Calculate full lighting if not emissive
            builder.makeExternalCall(
                "calcLighting", {
                    getParamValue(Parameter::eColor),
                    builder.makeCapabilityAccess(MaterialCapability::kVertexWorldPos),
                    getParamValue(Parameter::eNormal),
                    builder.makeCapabilityAccess(MaterialCapability::kCameraWorldPos),
                    builder.makeExternalCall("MaterialParams", {
                        getParamValue(Parameter::eSpecularFactor),
                        getParamValue(Parameter::eRoughness),
                        getParamValue(Parameter::eMetallicness),
                    })
                }
            )
        )
    );

    return shader::ShaderModuleCompiler{}.compile(
        out,
        std::move(builder),
        makeRayHitCapabilityConfig()
    );
}

auto FragmentModule::getParamValue(Parameter param) -> code::Value
{
    const auto index = static_cast<size_t>(param);
    if (!parameters[index].has_value())
    {
        throw std::invalid_argument(
            "[In FragmentShader::build]: A transparent material requires a value for the "
            + to_string(param) + " parameter, but none was specified.");
    }

    return parameters[index].value();
}

void FragmentModule::fillDefaultValues(shader::ShaderModuleBuilder& builder)
{
    auto tryFill = [&](Parameter param, shader::Constant constant) {
        const auto index = static_cast<size_t>(param);
        if (!parameters[index]) {
            parameters[index] = builder.makeConstant(constant);
        }
    };

    tryFill(Parameter::eColor,          vec4(1.0f));
    tryFill(Parameter::eNormal,         vec3(0, 0, 1));
    tryFill(Parameter::eSpecularFactor, 1.0f);
    tryFill(Parameter::eMetallicness,   0.0f);
    tryFill(Parameter::eRoughness,      1.0f);
    tryFill(Parameter::eEmissive,       false);
}

} // namespace trc
