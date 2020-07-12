#pragma once

#include <vulkan/vulkan.hpp>

#include "FrameSpecificObject.h"

namespace vkb
{
    using memptr = uint8_t*;

    struct BufferRegion
    {
        vk::DeviceSize offset{ 0 };
        vk::DeviceSize size{ VK_WHOLE_SIZE };
    };

    /**
     * @brief A buffer backed by managed device memory
     */
    class Buffer
    {
    public:
        /**
         * @brief Construct a buffer
         *
         *
         */
        Buffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags flags);

        Buffer(const Device& device,
               vk::DeviceSize bufferSize,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags flags);

        Buffer(vk::DeviceSize bufferSize,
               const void* data,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags flags);

        Buffer(const Device& device,
               vk::DeviceSize bufferSize,
               const void* data,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags flags);

        template<typename T>
        Buffer(const std::vector<T>& data, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags flags)
            : Buffer(vkb::VulkanBase::getDevice(), data, usage, flags)
        {}

        template<typename T>
        Buffer(const Device& device,
               const std::vector<T>& data,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags flags)
            : Buffer(device, sizeof(T) * data.size(), data.data(), usage, flags)
        {}

        Buffer(const Buffer&) = delete;
        Buffer(Buffer&&) = default;
        ~Buffer() noexcept = default;

        Buffer& operator=(const Buffer&) = delete;
        Buffer& operator=(Buffer&&) = default;

        auto inline operator*() const noexcept -> vk::Buffer {
            return *buffer;
        }

        inline auto get() const noexcept -> vk::Buffer {
            return *buffer;
        }

        /**
         * @brief Map a range of the buffer to CPU memory
         *
         * Size may be VK_WHOLE_SIZE to map the whole buffer.
         *
         * @param vk::DeviceSize offset Offset into the buffer in bytes
         * @param vk::DeviceSize size   Number of bytes mapped starting at the
         *                              specified offset. Must be less than or
         *                              equal to the total buffer size.
         */
        auto map(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) -> memptr;

        /**
         * @brief Map a range of the buffer to CPU memory
         *
         * @param BufferRegion mappedRegion Region of the buffer to map.
         */
        auto map(BufferRegion mappedRegion) -> memptr;

        /**
         * @brief Unmap the buffer if its memory is currently host mapped
         */
        void unmap();

        void copyFrom(const Buffer& src);
        void copyTo(const Buffer& dst);

        void copyFrom(vk::DeviceSize copySize, const void* data, BufferRegion dstRegion = {});

        template<typename T>
        inline void copyFrom(const T& data, BufferRegion dstRegion) {
            copyFrom(sizeof(T), &data, dstRegion);
        }

        template<typename T>
        inline void copyFrom(const std::vector<T>& data, BufferRegion dstRegion) {
            copyFrom(sizeof(T) * data.size(), data.data(), dstRegion);
        }

    private:
        const Device* device;

        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;

        vk::DeviceSize bufferSize{ 0 };
        mutable bool isMapped{ false };
    };


    /**
     * A buffer intended for transfer operations.
     *  - Host visible & coherent
     *  - Transfer dst & src
     */
    class CopyBuffer : public Buffer
    {
    public:
        explicit CopyBuffer(vk::DeviceSize bufferSize)
            : Buffer(
                bufferSize,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
            )
        {}
    };


    /**
     * A high-performance device-local buffer.
     * Cannot be copied to or from, cannot be mapped. Thus, must and can only
     * be assigned at creation time.
     */
    class DeviceLocalBuffer : private Buffer
    {
    public:
        DeviceLocalBuffer(vk::DeviceSize bufferSize, const void* data, vk::BufferUsageFlags usage);

        DeviceLocalBuffer(const Device& device,
                          vk::DeviceSize bufferSize,
                          const void* data,
                          vk::BufferUsageFlags usage);

        template<typename T>
        DeviceLocalBuffer(const std::vector<T>& data, vk::BufferUsageFlags usage)
            : DeviceLocalBuffer(sizeof(T) * data.size(), data.data(), usage)
        {}

        inline auto operator*() const noexcept -> vk::Buffer {
            return Buffer::get();
        }

        inline auto get() const noexcept -> vk::Buffer {
            return Buffer::get();
        }
    };



    // ---------------------------- //
    //        Helper functions        //
    // ---------------------------- //

    void copyBuffer(
        const Device& device,
        const vk::Buffer& dst, const vk::Buffer& src,
        vk::DeviceSize dstOffset, vk::DeviceSize srcOffset, vk::DeviceSize size
    );
} // namespace vkb
