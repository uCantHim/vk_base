#pragma once

#include "trc/core/PipelineLayoutTemplate.h"
#include "trc/core/PipelineTemplate.h"
#include "trc/core/RenderPassRegistry.h"
#include "trc/material/shader/ShaderProgram.h"

namespace trc
{
    struct DeviceExecutionContext;

    class MaterialRuntime;
    class MaterialProgram;

    /**
     * @brief Create a pipeline layout configuration for the shader program
     *
     * The program knows everything that's necessary to create a pipeline
     * layout compatible to it.
     *
     * @return PipelineLayoutTemplate
     */
    auto makePipelineLayout(const shader::ShaderProgramData& program)
        -> PipelineLayoutTemplate;

    class MaterialProgram
    {
    public:
        MaterialProgram(const shader::ShaderProgramData& data,
                        const PipelineDefinitionData& pipelineConfig,
                        const RenderPassDefinition& renderPass);

        auto getPipeline() const -> Pipeline::ID;

        auto getRuntime() const -> s_ptr<MaterialRuntime>;
        auto cloneRuntime() const -> u_ptr<MaterialRuntime>;

    private:
        PipelineLayout::ID layout;
        Pipeline::ID pipeline;

        std::vector<s_ptr<shader::ShaderRuntimeConstant>> runtimeValues;  // Keep the objects alive

        // The default runtime for this shader program.
        u_ptr<MaterialRuntime> rootRuntime;
    };

    class MaterialRuntime : public shader::ShaderProgramRuntime
    {
    public:
        MaterialRuntime(const shader::ShaderProgramData& program, MaterialProgram& prog);

        void bind(vk::CommandBuffer cmdBuf, DeviceExecutionContext& ctx);
        auto getPipeline() const -> Pipeline::ID;

    private:
        Pipeline::ID pipeline;
    };
} // namespace trc
