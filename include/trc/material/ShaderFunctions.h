#pragma once

#include "trc/material/FragmentShader.h"
#include "trc/material/shader/ShaderFunction.h"
#include "trc/material/shader/ShaderModuleBuilder.h"

namespace trc
{
    namespace code = shader::code;

    class TextureSample : public shader::ShaderFunction
    {
    public:
        TextureSample()
            : ShaderFunction("sampleTexture2D", shader::FunctionType{ { ui32{}, vec2{} }, vec4{} })
        {}

        void build(shader::ShaderModuleBuilder& builder,
                   const std::vector<code::Value>& args) override
        {
            auto textures = builder.makeCapabilityAccess(MaterialCapability::kTextureSample);
            builder.makeReturn(
                builder.makeExternalCall(
                    "texture",
                    { builder.makeArrayAccess(textures, args.at(0)), args.at(1) }
                )
            );
        }
    };

    class TangentToWorldspace : public shader::ShaderFunction
    {
    public:
        TangentToWorldspace()
            : ShaderFunction("TangentspaceToWorldspace", shader::FunctionType{ { vec3{} }, vec3{} })
        {}

        void build(shader::ShaderModuleBuilder& builder,
                   const std::vector<code::Value>& args) override
        {
            auto movedInterval = builder.makeSub(
                builder.makeMul(args[0], builder.makeConstant(2.0f)),
                builder.makeConstant(1.0f)
            );
            builder.makeReturn(
                builder.makeMul(
                    builder.makeCapabilityAccess(MaterialCapability::kTangentToWorldSpaceMatrix),
                    movedInterval
                )
            );
        }
    };
} // namespace trc
