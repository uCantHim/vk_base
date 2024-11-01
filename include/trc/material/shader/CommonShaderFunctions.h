#pragma once

#include <vector>

#include "ShaderFunction.h"
#include "ShaderModuleBuilder.h"

namespace trc::shader
{
    template<ui32 N, typename T>
    class Mix : public ShaderFunction
    {
    public:
        Mix();

        void build(ShaderModuleBuilder& builder, const std::vector<code::Value>& args) override {
            builder.makeReturn(builder.makeExternalCall("mix", args));
        }
    };

    template<ui32 N, typename T>
    Mix<N, T>::Mix()
        :
        ShaderFunction(
            "Mix",
            FunctionType{
                { glm::vec<N, T>{}, glm::vec<N, T>{}, float{} },
                glm::vec<N, T>{}
            }
        )
    {}
} // namespace trc::shader
