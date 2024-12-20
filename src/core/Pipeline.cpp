#include "trc/core/Pipeline.h"

#include <trc_util/Exception.h>

#include "trc/base/ShaderProgram.h"



trc::Pipeline::Pipeline(
    PipelineLayout& layout,
    UniquePipelineHandleType pipeline,
    vk::PipelineBindPoint bindPoint)
    :
    layout(&layout),
    pipelineStorage(std::move(pipeline)),
    bindPoint(bindPoint)
{
    if (!layout) {
        throw Exception("[In Pipeline::Pipeline]: Specified layout is not a valid layout handle");
    }

    using UniquePipelineDl = vk::UniqueHandle<vk::Pipeline, VulkanDispatchLoaderDynamic>;

    if (std::holds_alternative<vk::UniquePipeline>(pipelineStorage)) {
        this->pipeline = *std::get<vk::UniquePipeline>(pipelineStorage);
    }
    else if (std::holds_alternative<UniquePipelineDl>(pipelineStorage)) {
        this->pipeline = *std::get<UniquePipelineDl>(pipelineStorage);
    }
}

auto trc::Pipeline::operator*() const noexcept -> vk::Pipeline
{
    return pipeline;
}

auto trc::Pipeline::get() const noexcept -> vk::Pipeline
{
    return pipeline;
}

void trc::Pipeline::bind(vk::CommandBuffer cmdBuf) const
{
    assert(layout != nullptr && *layout);

    cmdBuf.bindPipeline(bindPoint, pipeline);
    layout->bindStaticDescriptorSets(cmdBuf, bindPoint);
    layout->bindDefaultPushConstantValues(cmdBuf);
}

void trc::Pipeline::bind(vk::CommandBuffer cmdBuf, const ResourceStorage& descStorage) const
{
    assert(layout != nullptr && *layout);

    cmdBuf.bindPipeline(bindPoint, pipeline);
    layout->bindStaticDescriptorSets(cmdBuf, bindPoint, descStorage);
    layout->bindDefaultPushConstantValues(cmdBuf);
}

auto trc::Pipeline::getLayout() noexcept -> PipelineLayout&
{
    assert(layout != nullptr);
    return *layout;
}

auto trc::Pipeline::getLayout() const noexcept -> const PipelineLayout&
{
    assert(layout != nullptr);
    return *layout;
}

auto trc::Pipeline::getBindPoint() const -> vk::PipelineBindPoint
{
    return bindPoint;
}



auto trc::makeComputePipeline(
    const Device& device,
    PipelineLayout& layout,
    const std::vector<ui32>& code,
    vk::PipelineCreateFlags flags,
    const std::string& entryPoint) -> Pipeline
{
    auto shaderModule = makeShaderModule(device, code);
    auto pipeline = device->createComputePipelineUnique(
        {},  // pipeline cache
        vk::ComputePipelineCreateInfo(
            flags,
            vk::PipelineShaderStageCreateInfo(
                {},
                vk::ShaderStageFlagBits::eCompute,
                *shaderModule,
                entryPoint.c_str()
            ),
            *layout
        )
    ).value;

    return Pipeline(layout, std::move(pipeline), vk::PipelineBindPoint::eCompute);
}
