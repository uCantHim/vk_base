#include "trc/core/DescriptorProvider.h"



trc::DescriptorProvider::DescriptorProvider(
    vk::DescriptorSetLayout layout,
    vk::DescriptorSet set)
    :
    layout(layout),
    set(set)
{
}

auto trc::DescriptorProvider::getDescriptorSetLayout() const noexcept -> vk::DescriptorSetLayout
{
    return layout;
}

void trc::DescriptorProvider::bindDescriptorSet(
    vk::CommandBuffer cmdBuf,
    vk::PipelineBindPoint bindPoint,
    vk::PipelineLayout pipelineLayout,
    ui32 setIndex) const
{
    cmdBuf.bindDescriptorSets(bindPoint, pipelineLayout, setIndex, set, {});
}

void trc::DescriptorProvider::setDescriptorSet(vk::DescriptorSet newSet)
{
    set = newSet;
}

void trc::DescriptorProvider::setDescriptorSetLayout(vk::DescriptorSetLayout newLayout)
{
    layout = newLayout;
}



trc::FrameSpecificDescriptorProvider::FrameSpecificDescriptorProvider(
    vk::DescriptorSetLayout layout,
    FrameSpecific<vk::DescriptorSet> set)
    :
    layout(layout),
    set(std::move(set))
{
}

auto trc::FrameSpecificDescriptorProvider::getDescriptorSetLayout() const noexcept
    -> vk::DescriptorSetLayout
{
    return layout;
}

void trc::FrameSpecificDescriptorProvider::bindDescriptorSet(
    vk::CommandBuffer cmdBuf,
    vk::PipelineBindPoint bindPoint,
    vk::PipelineLayout pipelineLayout,
    ui32 setIndex) const
{
    cmdBuf.bindDescriptorSets(bindPoint, pipelineLayout, setIndex, *set, {});
}

void trc::FrameSpecificDescriptorProvider::setDescriptorSet(
    FrameSpecific<vk::DescriptorSet> newSet)
{
    set = std::move(newSet);
}

void trc::FrameSpecificDescriptorProvider::setDescriptorSetLayout(
    vk::DescriptorSetLayout newLayout)
{
    layout = newLayout;
}
