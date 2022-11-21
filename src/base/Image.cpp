#include "trc/base/Image.h"

#include "trc/base/Buffer.h"
#include "trc/base/Barriers.h"



trc::Image::Image(
    const trc::Device& device,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageUsageFlags usage,
    const DeviceMemoryAllocator& alloc)
    :
    Image(
        device,
        vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            format,
            { width, height, 1 },
            1, 1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            usage
        ),
        alloc
    )
{
}

trc::Image::Image(
    const Device& device,
    const vk::ImageCreateInfo& createInfo,
    const DeviceMemoryAllocator& allocator)
    :
    device(&device)
{
    createNewImage(device, createInfo, allocator);
}

auto trc::Image::operator*() const noexcept -> vk::Image
{
    return *image;
}

auto trc::Image::getType() const noexcept -> vk::ImageType
{
    return type;
}

auto trc::Image::getFormat() const noexcept -> vk::Format
{
    return format;
}

auto trc::Image::getSize() const noexcept -> glm::uvec2
{
    return { extent.width, extent.height };
}

auto trc::Image::getExtent() const noexcept -> vk::Extent3D
{
    return extent;
}

void trc::Image::barrier(
    vk::CommandBuffer cmdBuf,
    vk::ImageLayout from,
    vk::ImageLayout to,
    vk::PipelineStageFlags srcStages,
    vk::PipelineStageFlags dstStages,
    vk::AccessFlags srcAccess,
    vk::AccessFlags dstAccess,
    vk::ImageSubresourceRange subRes)
{
    imageMemoryBarrier(cmdBuf,
        *image, from, to,
        srcStages, dstStages, srcAccess, dstAccess,
        subRes
    );
}

void trc::Image::writeData(
    const void* srcData,
    size_t srcSize,
    ImageSize destArea,
    vk::ImageLayout finalLayout)
{
    Buffer buf(
        *device,
        srcSize, srcData,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
    );

    device->executeCommands(QueueType::transfer, [&](auto cmdBuf)
    {
        barrier(cmdBuf,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eMemoryWrite,
            vk::AccessFlagBits::eMemoryWrite
        );
        const auto copyExtent = expandExtent(destArea.extent);
        cmdBuf.copyBufferToImage(
            *buf, *image,
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy(0, 0, 0, destArea.subres, destArea.offset, copyExtent)
        );
        barrier(cmdBuf,
            vk::ImageLayout::eTransferDstOptimal,
            finalLayout,
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eMemoryWrite,
            vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead
        );
    });
}

auto trc::Image::getMemory() const noexcept -> const DeviceMemory&
{
    return memory;
}

auto trc::Image::getDefaultSampler() const -> vk::Sampler
{
    if (!defaultSampler)
    {
        defaultSampler = device->get().createSamplerUnique(
            vk::SamplerCreateInfo(
                vk::SamplerCreateFlags(),
                vk::Filter::eLinear, vk::Filter::eLinear,
                vk::SamplerMipmapMode::eLinear,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat,
                vk::SamplerAddressMode::eRepeat
            )
        );
    }

    return *defaultSampler;
}

auto trc::Image::createView(vk::ImageAspectFlags aspect) const -> vk::UniqueImageView
{
    return device->get().createImageViewUnique(
        vk::ImageViewCreateInfo(
            {}, *image,
            type == vk::ImageType::e2D ? vk::ImageViewType::e2D
                : type == vk::ImageType::e3D ? vk::ImageViewType::e3D
                : vk::ImageViewType::e1D,
            format,
            {}, // component mapping
            vk::ImageSubresourceRange(aspect, 0, 1, 0, 1)
        )
    );
}

auto trc::Image::createView(
    vk::ImageViewType viewType,
    vk::Format viewFormat,
    vk::ComponentMapping componentMapping,
    vk::ImageSubresourceRange subRes) const -> vk::UniqueImageView
{
    return device->get().createImageViewUnique(
        { {}, *image, viewType, viewFormat, componentMapping, subRes }
    );
}

auto trc::Image::expandExtent(vk::Extent3D otherExtent) -> vk::Extent3D
{
    return {
        otherExtent.width  == UINT32_MAX ? extent.width  : otherExtent.width,
        otherExtent.height == UINT32_MAX ? extent.height : otherExtent.height,
        otherExtent.depth  == UINT32_MAX ? extent.depth  : otherExtent.depth
    };
}

void trc::Image::createNewImage(
    const Device& device,
    const vk::ImageCreateInfo& createInfo,
    const DeviceMemoryAllocator& allocator)
{
    image = device->createImageUnique(createInfo);
    auto memReq = device->getImageMemoryRequirements(*image);
    memory = allocator(device, vk::MemoryPropertyFlagBits::eDeviceLocal, memReq);
    memory.bindToImage(device, *image);

    type = createInfo.imageType;
    format = createInfo.format;
    extent = createInfo.extent;
}