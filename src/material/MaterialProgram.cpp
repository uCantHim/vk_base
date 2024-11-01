#include "trc/material/MaterialProgram.h"

#include "trc/core/DeviceTask.h"
#include "trc/core/ResourceConfig.h"
#include "trc/core/Pipeline.h"



namespace trc
{

auto makePipelineLayout(const shader::ShaderProgramData& program)
    -> PipelineLayoutTemplate
{
    // Convert push constant ranges to PipelineLayoutTemplate's format
    std::vector<PipelineLayoutTemplate::PushConstant> pushConstants;
    pushConstants.reserve(program.pcRangesPerStage.size());
    for (const auto& [stage, range] : program.pcRangesPerStage)
    {
        pushConstants.push_back({
            .range=range,
            .defaultValue=std::nullopt
        });
    }

    // Convert descriptors to PipelineLayoutTemplate's format
    std::vector<PipelineLayoutTemplate::Descriptor> descriptors;
    descriptors.reserve(program.descriptorSets.size());
    for (const auto& desc : program.descriptorSets) {
        descriptors.push_back(PipelineLayoutTemplate::Descriptor{ {desc.name}, true });
    }

    return PipelineLayoutTemplate{ descriptors, pushConstants };
}



MaterialProgram::MaterialProgram(
    const shader::ShaderProgramData& data,
    const PipelineDefinitionData& pipelineConfig,
    const RenderPassDefinition& renderPass)
    :
    layout(PipelineRegistry::registerPipelineLayout(makePipelineLayout(data))),
    rootRuntime(nullptr)
{
    // Create shader program
    ProgramDefinitionData program;
    for (const auto& [stage, code] : data.spirvCode) {
        program.stages.emplace(stage, ProgramDefinitionData::ShaderStage{ code });
    }

    // Load and set specialization constants
    for (const auto& [stageType, specs] : data.specConstants)
    {
        auto& stage = program.stages.at(stageType);
        for (const auto& [specIdx, specValue] : specs)
        {
            // Store the value provider (in case it wants to keep some data alive)
            runtimeValues.emplace_back(specValue);

            // Set specialization constant
            const auto data = specValue->loadData();
            assert(data.size() == specValue->getType().size());

            stage.specConstants.set(specIdx, data.data(), data.size());
        }
    }

    // Create pipeline
    pipeline = PipelineRegistry::registerPipeline(
        PipelineTemplate{ program, pipelineConfig },
        layout,
        renderPass
    );

    rootRuntime = std::make_unique<MaterialRuntime>(data, *this);
}

auto MaterialProgram::getPipeline() const -> Pipeline::ID
{
    return pipeline;
}

auto MaterialProgram::cloneRuntime() const -> u_ptr<MaterialRuntime>
{
    return std::make_unique<MaterialRuntime>(*rootRuntime);
}



MaterialRuntime::MaterialRuntime(const shader::ShaderProgramData& program, MaterialProgram& prog)
    :
    ShaderProgramRuntime(program),
    pipeline(prog.getPipeline())
{
}

void MaterialRuntime::bind(vk::CommandBuffer cmdBuf, DeviceExecutionContext& ctx)
{
    auto& p = ctx.resources().getPipeline(pipeline);
    p.bind(cmdBuf, ctx.resources());
    shader::ShaderProgramRuntime::uploadPushConstantDefaultValues(cmdBuf, *p.getLayout());
}

auto MaterialRuntime::getPipeline() const -> Pipeline::ID
{
    return pipeline;
}

} // namespace trc
