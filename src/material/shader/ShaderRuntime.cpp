#include "trc/material/shader/ShaderRuntime.h"

#include <algorithm>

#include "trc/material/shader/ShaderProgram.h"



namespace trc::shader
{

ShaderProgramRuntime::ShaderProgramRuntime(const ShaderProgramData& program)
    :
    pc(std::make_shared<std::vector<PushConstant>>()),
    descriptorSetIndices(std::make_shared<std::unordered_map<std::string, ui32>>())
{
    for (auto [offset, size, stages, userId] : program.pushConstants)
    {
        constexpr PushConstant alloc{ .offset=kUserIdNotUsed, .stages={} };
        pc->resize(std::max(size_t{userId + 1}, pc->size()), alloc);

        pc->at(userId).offset = offset;
        pc->at(userId).stages = stages;
    }

    for (const auto& [desc, index] : program.descriptorSets) {
        descriptorSetIndices->try_emplace(desc, index);
    }
}

auto ShaderProgramRuntime::clone() const -> u_ptr<ShaderProgramRuntime>
{
    return std::make_unique<ShaderProgramRuntime>(*this);
}

bool ShaderProgramRuntime::hasPushConstant(ui32 pushConstantId) const
{
    return pc->size() > pushConstantId
        && pc->at(pushConstantId).offset != kUserIdNotUsed;
}

void ShaderProgramRuntime::pushConstants(
    vk::CommandBuffer cmdBuf,
    vk::PipelineLayout layout,
    ui32 pushConstantId,
    const void* data, size_t size) const
{
    assert_arg(hasPushConstant(pushConstantId));
    cmdBuf.pushConstants(layout, pc->at(pushConstantId).stages,
                         pc->at(pushConstantId).offset,
                         size, data);
}

void ShaderProgramRuntime::setPushConstantDefaultValue(
    ui32 pushConstantId,
    std::span<const std::byte> data)
{
    pushConstantData.emplace_back(
        pushConstantId,
        std::vector<std::byte>{ data.begin(), data.end() }
    );
}

void ShaderProgramRuntime::uploadPushConstantDefaultValues(
    vk::CommandBuffer cmdBuf,
    vk::PipelineLayout layout)
{
    for (const auto& [id, data] : pushConstantData) {
        pushConstants(cmdBuf, layout, id, data.data(), data.size());
    }
}

auto ShaderProgramRuntime::getDescriptorSetIndex(const std::string& name) const
    -> std::optional<ui32>
{
    auto it = descriptorSetIndices->find(name);
    if (it != descriptorSetIndices->end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace trc::shader
