#include "core/ComputePipelineBuilder.h"



trc::ComputePipelineBuilder::ComputePipelineBuilder(const ComputePipelineTemplate& _template)
    : _template(_template)
{
}

auto trc::ComputePipelineBuilder::setProgram(ShaderCode newCode) -> Self&
{
    _template.setProgramCode(std::move(newCode));
    return *this;
}

auto trc::ComputePipelineBuilder::build() const -> ComputePipelineTemplate
{
    return _template;
}

auto trc::ComputePipelineBuilder::build(const vkb::Device& device, PipelineLayout& layout)
    -> Pipeline
{
    return makeComputePipeline(device, build(), layout);
}



auto trc::buildComputePipeline() -> ComputePipelineBuilder
{
    return ComputePipelineBuilder{};
}