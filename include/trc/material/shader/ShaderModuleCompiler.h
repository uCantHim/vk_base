#pragma once

#include <cassert>

#include <string>

#include "ShaderModuleBuilder.h"
#include "ShaderOutputInterface.h"
#include "ShaderResourceInterface.h"

namespace trc::shader
{
    /**
     * Holds information about a compiled shader module.
     */
    struct ShaderModule : ShaderResourceInterface
    {
        ShaderModule(const ShaderModule&) = default;
        ShaderModule(ShaderModule&&) = default;
        ShaderModule& operator=(const ShaderModule&) noexcept = default;
        ShaderModule& operator=(ShaderModule&&) noexcept = default;
        ~ShaderModule() noexcept = default;

        /**
         * @return std::string The GLSL code for the entire shader module.
         */
        auto getShaderCode() const -> const std::string&;

    private:
        friend class ShaderModuleCompiler;

        ShaderModule(std::string shaderCode, ShaderResourceInterface resourceInfo);

        std::string shaderGlslCode;
    };

    class ShaderModuleCompiler
    {
    public:
        /**
         * @brief Compile a full shader module
         *
         * Compile resource requirements, function definitions, and output value
         * declarations into a shader module.
         *
         * Queries or creates a function "main" and appends output code
         * (assignments, function calls, ...) to it's block.
         */
        static auto compile(const ShaderOutputInterface& output,
                            ShaderModuleBuilder builder,
                            const CapabilityConfig& caps)
            -> ShaderModule;

    private:
        static auto compileSettings(const ShaderModuleBuilder::Settings& settings) -> std::string;
    };
} // namespace trc::shader
