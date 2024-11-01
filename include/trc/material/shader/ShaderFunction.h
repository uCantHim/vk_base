#pragma once

#include <string>
#include <vector>

#include "CodePrimitives.h"

namespace trc::shader
{
    class ShaderModuleBuilder;

    /**
     * A self-contained, typed mechanism to create shader functions. A
     * subclass can implement one GLSL function. Add this function to the
     * code builder with `ShaderModuleBuilder::makeCall`.
     */
    class ShaderFunction
    {
    public:
        virtual ~ShaderFunction() noexcept = default;

        ShaderFunction(const std::string& name, FunctionType signature);

        // TODO (maybe): Don't require the function to create a return statement - return
        // the returned value from this function.
        virtual void build(ShaderModuleBuilder& builder, const std::vector<code::Value>& args) = 0;

        auto getName() const -> const std::string&;
        auto getType() const -> const FunctionType&;

    private:
        const std::string name;
        const FunctionType signature;
    };
} // namespace trc::shader
