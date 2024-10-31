#pragma once

#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Constant.h"
#include "ShaderCapabilities.h"
#include "ShaderCapabilityConfig.h"
#include "ShaderCodePrimitives.h"
#include "ShaderRuntimeConstant.h"

namespace trc
{
    /**
     * @brief A collection of resources used by a shader module
     *
     * This is a result of shader module generation. A `ShaderResources` object
     * defines all resources used by a shader module and can generate shader
     * code that defines these resources correctly.
     *
     * Additionally, it provides an interface for inspection of the requested
     * resources' properties.
     */
    class ShaderResources
    {
    public:
        using ResourceID = ShaderCapabilityConfig::ResourceID;

        struct SpecializationConstantInfo
        {
            s_ptr<ShaderRuntimeConstant> value;

            // The index of the specialization constant that will hold the
            // runtime data.
            ui32 specializationConstantIndex;
        };

        struct ShaderInputInfo
        {
            // The input data's input location as in
            // `layout (location = X) in ...`
            ui32 location;

            // Data type of the shader input.
            BasicType type;

            // Name of the shader input variable in the generated shader code.
            std::string variableName;

            // The declaration code for the resource.
            // Usually only used internally.
            std::string declCode;

            // The capability in the corresponding `ShaderCapabilityConfig`
            // that defines this shader input.
            Capability capability;
        };

        struct PushConstantInfo
        {
            ui32 offset;
            ui32 size;

            // The push constant value's runtime handle as specified by the user.
            //
            // Used to interface with `MaterialRuntime::pushConstants`.
            ui32 userId;

            // A placeholder variable in the generated GLSL code. This must
            // be replaced by the final byte offset of the push constant in the
            // full shader program.
            std::string offsetPlaceholder;
        };

        struct PayloadInfo
        {
            code::Type type;

            // The capability that requested this resource
            Capability capability;

            // Name of the placeholder variable for the payload's location
            std::string locationPlaceholder;
        };

        ShaderResources() = default;

        /**
         * @brief Get all resource definitions as GLSL code
         *
         * This is only the code for the shader module's resource definitions
         * (descriptor set declarations, shader input locations, ...), NOT the
         * full code of the shader module! The shader logic and the resources
         * it uses are decoupled and handled by `ShaderCodeBuilder` and
         * `ShaderResourceInterface`, respectively.
         *
         * @return const std::string&
         */
        auto getGlslCode() const -> const std::string&;

        /**
         * @brief Query required shader inputs
         *
         * @return std::vector<ShaderInputInfo> A list of all input locations
         *                                      required by the shader module.
         */
        auto getRequiredShaderInputs() const
            -> const std::vector<ShaderInputInfo>&;

        /**
         * Structs { <value>, <spec-idx> }
         *
         * The required operation at pipeline creation is:
         *
         *     specConstants[<spec-idx>] = <value>->loadData();
         */
        auto getSpecializationConstants() const -> const std::vector<SpecializationConstantInfo>&;

        /**
         * @brief Query a list of all descriptor sets required by this shader
         *        module
         *
         * Descriptor sets are defined by a string identifier.
         *
         * @return std::vector<std::string> All descriptor sets used by the
         *                                  shader module. Entries are always
         *                                  unique, but unordered.
         */
        auto getRequiredDescriptorSets() const -> std::vector<std::string>;

        /**
         * @brief Get the index-placeholder variable for a descriptor set
         *
         * Because descriptor set indices are generated *after* shader code
         * generation, the resource code (`ShaderResources::getGlslCode`)
         * contains placeholder variables for these indices. They can be found
         * and replaced with the `ShaderDocument` utility.
         *
         * # Example
         *
         * ```cpp
         * #include <shader_tools/ShaderDocument.h>
         *
         * ShaderResources res = ...;
         *
         * shader_edit::ShaderDocument doc(res.getGlslCode());
         * doc.set(*res.getDescriptorIndexPlaceholder("my_desc_set"), 0);
         * doc.set(*res.getDescriptorIndexPlaceholder("second_set"), 1);
         *
         * auto resourceCode = doc.compile();
         * ```
         *
         * @return std::optional<std::string> The name of `setName`'s placeholder
         *         variable. nullopt if the descriptor set `setName` is not
         *         required by this shader module.
         */
        auto getDescriptorIndexPlaceholder(const std::string& setName) const
            -> std::optional<std::string>;

        /**
         * @return ui32 Total size of the stage's push constants
         */
        auto getPushConstantSize() const -> ui32;

        /**
         * The push constant ranges returned will include possible the base
         * offset specified via `setPushConstantBaseOffset`, if any.
         *
         * @return std::vector<PushConstantInfo> All push constants used by the
         *                                       shader module.
         */
        auto getPushConstants() const -> std::vector<PushConstantInfo>;

        /**
         * @brief Get the offset-placeholder variable for a push constant
         *
         * @param ui32 pushConstantId A user ID that identifies a push constant.
         *
         * # Example
         *
         * ```cpp
         * #include <shader_tools/ShaderDocument.h>
         *
         * ShaderResources res = ...;
         *
         * shader_edit::ShaderDocument doc(res.getGlslCode());
         * doc.set(*res.getPushConstantOffsetPlaceholder(kPcVertex), 0);
         * doc.set(*res.getPushConstantOffsetPlaceholder(kPcFragment), 64);
         *
         * auto resourceCode = doc.compile();
         * ```
         *
         * @return std::optional<std::string> The name of the placeholder
         *         variable for the push constant's byte offset. nullopt if the
         *         push constant is not required by the shader module or if no
         *         push constant with the specified ID exists.
         */
        auto getPushConstantOffsetPlaceholder(ui32 pushConstantId) const
            -> std::optional<std::string>;

        auto getRequiredPayloads() const -> const std::vector<PayloadInfo>&;

    private:
        friend class ShaderResourceInterface;

        std::string code;

        std::vector<ShaderInputInfo> requiredShaderInputs;
        std::vector<PayloadInfo> requiredPayloads;

        std::vector<SpecializationConstantInfo> specConstants;
        std::unordered_map<std::string, std::string> descriptorSetIndexPlaceholders;

        // Map { pcUserId -> PushConstantInfo }
        std::unordered_map<ui32, PushConstantInfo> pushConstantInfos;
        ui32 pushConstantSize;
    };

    /**
     * @brief Interface to a shader module's resources
     *
     * Ties the `ShaderCapabilityConfig` and the `ShaderCodeBuilder` together.
     *
     * The code builder can query capabilities in the form of code values.
     * `ShaderResourceInterface` collects all resources queried this way and
     * finally generates the shader code that defines these resources.
     *
     * TODO: This should be called 'ShaderResourceInterfaceBuilder'. 'ShaderResources' should
     *       be called 'ShaderResourceInterface' instead.
     */
    class ShaderResourceInterface
    {
    public:
        ShaderResourceInterface(const ShaderCapabilityConfig& config,
                                ShaderCodeBuilder& codeBuilder);

        /**
         * @brief Directly query a capability
         *
         * This might throw if the capability is not suited for direct
         * access. An example is the texture sample capability, which
         * requires an additional argument to be accessed.
         */
        auto queryCapability(Capability capability) -> code::Value;

        auto makeSpecConstant(s_ptr<ShaderRuntimeConstant> value) -> code::Value;

        /**
         * @brief Compile requested resources to shader code
         */
        auto compile() const -> ShaderResources;

    private:
        struct DescriptorBindingFactory
        {
            auto make(const ShaderCapabilityConfig::DescriptorBinding& binding) -> std::string;

            auto getCode() const -> const std::string&;
            auto getDescriptorSets() const -> std::unordered_map<std::string, std::string>;

        private:
            auto getDescriptorSetPlaceholder(const std::string& set) -> std::string;

            ui32 nextNameIndex{ 0 };

            std::unordered_map<std::string, std::string> descriptorSetPlaceholders;
            std::string generatedCode;
        };

        struct PushConstantFactory
        {
            using ResourceID = ShaderResources::ResourceID;
            using PushConstantInfo = ShaderResources::PushConstantInfo;

            auto make(ResourceID resource, const ShaderCapabilityConfig::PushConstant& pc)
                -> std::string;

            auto getTotalSize() const -> ui32;
            auto getInfos() const -> const std::unordered_map<ResourceID, PushConstantInfo>&;

            auto getCode() const -> std::string;

        private:
            static constexpr auto kPcBlockName{ "PushConstants" };
            static constexpr auto kPcBlockNamespaceName{ "pushConstants" };

            ui32 totalSize{ 0 };
            // Map { pcUserId -> PushConstantInfo }
            std::unordered_map<ui32, PushConstantInfo> infos;

            std::string code;
        };

        struct ShaderInputFactory
        {
            auto make(Capability capability, const ShaderCapabilityConfig::ShaderInput& in)
                -> std::string;

            ui32 nextShaderInputLocation{ 0 };
            std::vector<ShaderResources::ShaderInputInfo> shaderInputs;
        };

        struct RayPayloadFactory
        {
            using ResourceID = ShaderCapabilityConfig::ResourceID;
            using PayloadInfo = ShaderResources::PayloadInfo;

            /** @return std::string The generated identifier for the payload */
            auto make(Capability capability, const ShaderCapabilityConfig::RayPayload& pl)
                -> std::string;

            auto getCode() const -> const std::string&;
            auto getPayloads() const -> const std::vector<PayloadInfo>&;

        private:
            ui32 nextNameIndex{ 0 };

            std::vector<PayloadInfo> payloads;
            std::string code;
        };

        struct HitAttributeFactory
        {
            auto make(const ShaderCapabilityConfig::HitAttribute& att) -> std::string;

            auto getCode() const -> const std::string&;

        private:
            ui32 nextNameIndex{ 0 };
            std::string code;
        };

        using Resource = const ShaderCapabilityConfig::ResourceData*;

        void requireResource(Capability capability, ShaderCapabilityConfig::ResourceID resource);

        const ShaderCapabilityConfig& config;
        ShaderCodeBuilder* codeBuilder;

        std::unordered_set<std::string> requiredExtensions;
        std::unordered_set<util::Pathlet> requiredIncludePaths;
        std::vector<std::pair<std::string, std::optional<std::string>>> requiredMacros;

        ui32 nextSpecConstantIndex{ 0 };
        /** Vector of pairs (index, name) */
        std::vector<std::pair<ui32, std::string>> specializationConstants;
        /** Map of pairs (index -> value) */
        std::unordered_map<ui32, s_ptr<ShaderRuntimeConstant>> specializationConstantValues;

        std::unordered_map<Resource, std::pair<std::string, std::string>> resourceMacros;

        DescriptorBindingFactory descriptorFactory;
        PushConstantFactory pushConstantFactory;
        ShaderInputFactory shaderInput;
        RayPayloadFactory rayPayloadFactory;
        HitAttributeFactory hitAttributeFactory;
    };
} // namespace trc
