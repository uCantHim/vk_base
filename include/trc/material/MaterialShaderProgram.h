#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "MaterialRuntime.h"
#include "ShaderModuleCompiler.h"
#include "ShaderResourceInterface.h"
#include "ShaderRuntimeConstant.h"
#include "material_shader_program.pb.h"
#include "trc/core/PipelineLayoutTemplate.h"
#include "trc/core/PipelineTemplate.h"
#include "trc/core/RenderPassRegistry.h"

namespace trc
{
    struct ShaderDescriptorConfig
    {
        struct DescriptorInfo
        {
            auto operator<=>(const DescriptorInfo&) const = default;

            ui32 index;
            bool isStatic;
        };

        std::unordered_map<std::string, DescriptorInfo> descriptorInfos;
    };

    /**
     * @brief A serializable representation of a full shader program
     *
     * Create `MaterialProgramData` structs with `linkMaterialProgram`. This
     * function performs quite complicated logic to collect and merge
     * information from a set of shader modules and one should never edit
     * `MaterialProgramData`'s fields manually.
     *
     * # Example
     * ```cpp
     *
     * ShaderModule myVertexModule = ...;
     * ShaderModule myFragmentModule = ...;
     *
     * MaterialProgramData myProgram = linkMaterialProgram(
     *     {
     *         { vk::ShaderStageFlagBits::eVertex, std::move(myVertexModule) },
     *         { vk::ShaderStageFlagBits::eFragment, std::move(myVertexModule) },
     *     },
     *     myDescriptorConfig
     * );
     * ```
     */
    struct MaterialProgramData
    {
        struct PushConstantRange
        {
            ui32 offset;
            ui32 size;
            vk::ShaderStageFlags shaderStages;

            ui32 userId;
        };

        std::unordered_map<vk::ShaderStageFlagBits, std::vector<ui32>> spirvCode;

        std::unordered_map<
            vk::ShaderStageFlagBits,
            std::vector<std::pair<ui32, s_ptr<ShaderRuntimeConstant>>>
        > specConstants;
        std::vector<PushConstantRange> pushConstants;
        std::vector<PipelineLayoutTemplate::Descriptor> descriptorSets;

        /**
         * @brief Create a pipeline layout configuration for the shader program
         *
         * The program knows everything that's necessary to create a pipeline
         * layout compatible to it.
         *
         * @return PipelineLayoutTemplate
         */
        auto makeLayout() const -> PipelineLayoutTemplate;

        auto serialize() const -> serial::ShaderProgram;
        void deserialize(const serial::ShaderProgram& program,
                         ShaderRuntimeConstantDeserializer& deserializer);

        void serialize(std::ostream& os) const;
        void deserialize(std::istream& is,
                         ShaderRuntimeConstantDeserializer& deserializer);
    };

    /**
     * @brief Create a shader program from a set of shader modules
     */
    auto linkMaterialProgram(std::unordered_map<vk::ShaderStageFlagBits, ShaderModule> modules,
                             const ShaderDescriptorConfig& descConfig)
        -> MaterialProgramData;

    /**
     * @brief A complete shader program with a pipeline and a runtime
     */
    class MaterialShaderProgram
    {
    public:
        MaterialShaderProgram(const MaterialProgramData& data,
                              const PipelineDefinitionData& pipelineConfig,
                              const RenderPassDefinition& renderPass);

        auto getLayout() const -> const PipelineLayoutTemplate&;
        auto makeRuntime() const -> MaterialRuntime;

    private:
        using ShaderStageMap = std::unordered_map<vk::ShaderStageFlagBits, ShaderModule>;

        PipelineLayoutTemplate layoutTemplate;
        PipelineLayout::ID layout;

        Pipeline::ID pipeline{ Pipeline::ID::NONE };
        s_ptr<std::vector<ui32>> runtimePcOffsets{ std::make_shared<std::vector<ui32>>() };
        s_ptr<std::vector<vk::ShaderStageFlags>> runtimePcStages{
            std::make_shared<std::vector<vk::ShaderStageFlags>>()
        };
        std::vector<s_ptr<ShaderRuntimeConstant>> runtimeValues;  // Just keep the objects alive
    };
} // namespace trc
