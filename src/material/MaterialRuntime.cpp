#include "trc/material/MaterialRuntime.h"



namespace trc
{

MaterialRuntime::MaterialRuntime(
    Pipeline::ID pipeline,
    const s_ptr<std::vector<ui32>>& pcOffsets,
    const s_ptr<std::vector<vk::ShaderStageFlags>>& pcStages)
    :
    pipeline(pipeline),
    pcOffsets(pcOffsets),
    pcShaderStages(pcStages)
{
}

auto MaterialRuntime::getPipeline() const -> Pipeline::ID
{
    return pipeline;
}

} // namespace trc
