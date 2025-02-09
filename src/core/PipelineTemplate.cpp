#include "trc/core/PipelineTemplate.h"

#include "trc/base/ShaderProgram.h"
#include "trc/core/Instance.h"



bool trc::SpecializationConstantStorage::empty() const
{
    return data.empty();
}

auto trc::SpecializationConstantStorage::makeSpecializationInfo() const -> vk::SpecializationInfo
{
    assert((entries.empty() && data.empty()) || (!entries.empty() && !data.empty()));

    return {
        static_cast<ui32>(entries.size()), entries.data(),
        static_cast<ui32>(data.size()), data.data()
    };
}

void trc::SpecializationConstantStorage::set(ui32 constantId, const void* dataPtr, size_t size)
{
    // Copy new entry's data
    const ui32 offset = data.size();
    const ui32 minSize = offset + size;
    assert(data.size() < minSize);

    data.resize(minSize);
    memcpy(data.data() + offset, dataPtr, size);

    // Add map entry
    entries.emplace_back(vk::SpecializationMapEntry(constantId, offset, size));
}



auto trc::ProgramDefinitionData::makeProgram(const Device& device) const
    -> ShaderProgram
{
    ShaderProgram program(device);

    for (const auto& [type, stage] : stages)
    {
        if (stage.specConstants.empty()) {
            program.addStage(type, stage.code);
        }
        else {
            program.addStage(type, stage.code, stage.specConstants.makeSpecializationInfo());
        }
    }

    return program;
}



trc::PipelineTemplate::PipelineTemplate(ProgramDefinitionData program, PipelineDefinitionData pipeline)
    :
    program(std::move(program)),
    data(std::move(pipeline))
{
}

auto trc::PipelineTemplate::getProgramData() const -> const ProgramDefinitionData&
{
    return program;
}

auto trc::PipelineTemplate::getPipelineData() const -> const PipelineDefinitionData&
{
    return data;
}



trc::ComputePipelineTemplate::ComputePipelineTemplate(std::vector<ui32> shaderCode)
    :
    shaderCode(std::move(shaderCode))
{
}

void trc::ComputePipelineTemplate::setProgramCode(std::vector<ui32> code)
{
    shaderCode = std::move(code);
}

auto trc::ComputePipelineTemplate::getShaderCode() const -> const std::vector<ui32>&
{
    return shaderCode;
}

auto trc::ComputePipelineTemplate::getSpecializationConstants() const
    -> const SpecializationConstantStorage&
{
    return specConstants;
}

auto trc::ComputePipelineTemplate::getEntryPoint() const -> const std::string&
{
    return entryPoint;
}



namespace trc
{

auto makeGraphicsPipeline(
    const Device& device,
    const PipelineTemplate& _template,
    PipelineLayout& layout,
    vk::RenderPass renderPass,
    ui32 subPass,
    const vk::PipelineRenderingCreateInfo& renderingInfo) -> Pipeline
{
    const PipelineDefinitionData& def = _template.getPipelineData();
    const ProgramDefinitionData& shader = _template.getProgramData();

    // Create a program from the shader code
    ShaderProgram program = shader.makeProgram(device);

    // Create some of the pipeline state info structs
    // The structs that refer to other resources stored in the pipeline definition data
    // are created here so that the definition data struct can be safely copied.
    std::vector<vk::DynamicState> dynamicStates = def.dynamicStates;
    vk::PipelineViewportStateCreateInfo viewport({}, def.viewports, def.scissorRects);
    vk::PipelineVertexInputStateCreateInfo vertexInput({}, def.inputBindings, def.attributes);
    auto colorBlending = def.colorBlending;
    colorBlending.setAttachments(def.colorBlendAttachments);

    // Set viewport/scissor as dynamic states if none are specified
    vk::Viewport defaultViewport(0, 0, 1, 1, 0.0f, 1.0f);
    vk::Rect2D defaultScissor({ 0, 0 }, { 1, 1 });
    if (viewport.viewportCount == 0)
    {
        viewport.setViewports(defaultViewport);
        if (std::ranges::find(dynamicStates, vk::DynamicState::eViewport) == dynamicStates.end()) {
            dynamicStates.emplace_back(vk::DynamicState::eViewport);
        }
    }
    if (viewport.scissorCount == 0)
    {
        viewport.setScissors(defaultScissor);
        if (std::ranges::find(dynamicStates, vk::DynamicState::eScissor) == dynamicStates.end()) {
            dynamicStates.emplace_back(vk::DynamicState::eScissor);
        }
    }
    vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

    // Create the pipeline
    vk::StructureChain createInfoChain{
        vk::GraphicsPipelineCreateInfo(
            {},
            program.getStageCreateInfo(),
            &vertexInput,
            &def.inputAssembly,
            &def.tessellation,
            &viewport,
            &def.rasterization,
            &def.multisampling,
            &def.depthStencil,
            &colorBlending,
            &dynamicState,
            *layout,
            renderPass, subPass,
            vk::Pipeline(), 0
        ),
        renderingInfo
    };
    if (renderPass != VK_NULL_HANDLE) {
        createInfoChain.unlink<vk::PipelineRenderingCreateInfo>();
    }

    auto pipeline = device->createGraphicsPipelineUnique({}, createInfoChain.get()).value;

    return Pipeline{ layout, std::move(pipeline), vk::PipelineBindPoint::eGraphics };
}

} // namespace trc

auto trc::makeGraphicsPipeline(
    const Device& device,
    const PipelineTemplate& _template,
    PipelineLayout& layout,
    vk::RenderPass renderPass,
    ui32 subPass) -> Pipeline
{
    return makeGraphicsPipeline(device, _template, layout, renderPass, subPass, {});
}

auto trc::makeGraphicsPipeline(
    const Device& device,
    const PipelineTemplate& _template,
    PipelineLayout& layout,
    const vk::PipelineRenderingCreateInfo& renderingInfo) -> Pipeline
{
    return makeGraphicsPipeline(device, _template, layout, {}, {}, renderingInfo);
}

auto trc::makeComputePipeline(
    const Device& device,
    const ComputePipelineTemplate& _template,
    PipelineLayout& layout) -> Pipeline
{
    auto shaderModule = makeShaderModule(device, _template.getShaderCode());
    auto pipeline = device->createComputePipelineUnique(
        {},
        vk::ComputePipelineCreateInfo(
            {},
            vk::PipelineShaderStageCreateInfo(
                {}, vk::ShaderStageFlagBits::eCompute,
                *shaderModule,
                _template.getEntryPoint().c_str()
            ),
            *layout
        )
    ).value;

    return Pipeline{ layout, std::move(pipeline), vk::PipelineBindPoint::eCompute };
}
