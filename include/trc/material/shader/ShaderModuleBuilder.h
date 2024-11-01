#pragma once

#include <concepts>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <spirv/FileIncluder.h>

#include "Capability.h"
#include "ShaderCodeBuilder.h"
#include "ShaderRuntimeConstant.h"
#include "ShaderFunction.h"

namespace trc::shader
{
    namespace fs = std::filesystem;

    class ResourceResolver;
    class CapabilityConfig;

    /**
     * An extension of ShaderCodeBuilder with additional functionality to
     * create and access shader resources.
     *
     * Builds complete shader modules.
     */
    class ShaderModuleBuilder : public ShaderCodeBuilder
    {
    public:
        ShaderModuleBuilder& operator=(const ShaderModuleBuilder&) = delete;
        ShaderModuleBuilder& operator=(ShaderModuleBuilder&&) noexcept = delete;

        ShaderModuleBuilder(const ShaderModuleBuilder&) = default;
        ShaderModuleBuilder(ShaderModuleBuilder&&) noexcept = default;
        ~ShaderModuleBuilder() noexcept = default;

        ShaderModuleBuilder() = default;

        template<std::derived_from<ShaderFunction> T>
            requires std::is_default_constructible_v<T>
        auto makeCall(std::vector<Value> args) -> Value;

        auto makeCall(ShaderFunction& func, std::vector<Value> args) -> Value;

        auto makeCapabilityAccess(Capability capability) -> Value;

        /**
         * @brief Create a specialization constant with a runtime value
         *
         * @param s_ptr<ShaderRuntimeConstant> value The specialization
         *        constant's value, determined at runtime.
         *
         * @return code::Value
         */
        auto makeSpecializationConstant(s_ptr<ShaderRuntimeConstant> value) -> Value;

        /**
         * @throw std::invalid_argument if `location` is already defined with a
         *                              conflicting type.
         */
        auto makeOutputLocation(ui32 location, BasicType type) -> Value;

        using ShaderCodeBuilder::makeCallStatement;

        /**
         * @brief Create a function call statement
         *
         * Since the generated instruction is not an expression, it will
         * never be optimized away if the result is not used. Use this to
         * call functions that are purely used for side effects, e.g. image
         * stores.
         */
        template<std::derived_from<ShaderFunction> T>
            requires std::is_default_constructible_v<T>
        void makeCallStatement(std::vector<code::Value> args);

        /**
         * @brief Create a function call statement
         *
         * Since the generated instruction is not an expression, it will
         * never be optimized away if the result is not used. Use this to
         * call functions that are purely used for side effects, e.g. image
         * stores.
         */
        void makeCallStatement(ShaderFunction& func, std::vector<code::Value> args);

        /**
         * @brief Include external code into the module
         *
         * The file contents are included after the resource declarations.
         * Multiple included files are included in the order in which
         * `includeCode` was called.
         *
         * The include path is resolved at module compile time.
         *
         * Allows the specification of additional replacement variables in
         * the included file and corresponding shader capabilities. The
         * variables will be replaced by the accessor to the respective
         * capability.
         *
         * @param fs::path path The path to the included file
         * @param map<string, Capability> Maps replacement-variable names
         *                                to capabilities.
         */
        void includeCode(fs::path path,
                         std::unordered_map<std::string, Capability> varReplacements);

        struct Settings
        {
            std::string versionString{ "460" };
            bool earlyFragmentTests{ false };
        };

        /**
         * @brief Set the string appended to the generated #version directive.
         *
         * Default is `#version 460`.
         */
        void setVersionString(std::string str);

        /**
         * Generate an instruction in the shader that enables early fragment
         * tests.
         *
         * Is disabled by default.
         */
        void enableEarlyFragmentTest();

        /**
         * Remove the instruction in the shader that enables early fragment
         * tests.
         *
         * Is disabled by default.
         */
        void disableEarlyFragmentTest();

        auto getSettings() const -> const Settings&;

        /**
         * @brief Generate code that declares the module's outputs.
         *
         * Append this to the resource declaration code.
         */
        auto compileOutputLocations() const -> std::string;

        /**
         * @brief Generate included code by reading included files.
         *
         * Search for included files, apply variable replacement to their
         * content if specified, and append their content in order.
         */
        auto compileIncludedCode(shaderc::CompileOptions::IncluderInterface& includer,
                                 ResourceResolver& resolver)
            -> std::string;

    private:
        template<std::derived_from<ShaderFunction> T>
            requires std::is_default_constructible_v<T>
        auto getOrMakeFunctionDef() -> Function;
        auto getOrMakeFunctionDef(ShaderFunction& func) -> Function;

        Settings shaderSettings;

        /** Maps [location -> { type, name }] */
        std::unordered_map<ui32, std::pair<BasicType, std::string>> outputLocations;

        // Keep the includes in insertion order
        std::vector<
            std::pair<fs::path, std::unordered_map<std::string, Value>>
        > includedFiles;
    };



    template<std::derived_from<ShaderFunction> T>
        requires std::is_default_constructible_v<T>
    auto ShaderModuleBuilder::getOrMakeFunctionDef() -> Function
    {
        T funcBuilder;
        return getOrMakeFunctionDef(funcBuilder);
    }

    template<std::derived_from<ShaderFunction> T>
        requires std::is_default_constructible_v<T>
    auto ShaderModuleBuilder::makeCall(std::vector<Value> args) -> Value
    {
        return ShaderCodeBuilder::makeCall(getOrMakeFunctionDef<T>(), std::move(args));
    }

    template<std::derived_from<ShaderFunction> T>
        requires std::is_default_constructible_v<T>
    void ShaderModuleBuilder::makeCallStatement(std::vector<code::Value> args)
    {
        T func;
        makeCallStatement(func, std::move(args));
    }
} // namespace trc::shader
