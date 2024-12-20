#pragma once

#include <variant>

#include "trc/base/Device.h"

#include "trc/Types.h"
#include "trc/core/PipelineLayout.h"

namespace trc
{
    class ResourceStorage;

    /**
     * @brief Base class for all pipelines
     */
    class Pipeline
    {
    public:
        using ID = TypesafeID<Pipeline, ui32>;
        using UniquePipelineHandleType = std::variant<
            vk::UniqueHandle<vk::Pipeline, VulkanDispatchLoaderStatic>,  // vk::UniquePipeline
            vk::UniqueHandle<vk::Pipeline, VulkanDispatchLoaderDynamic>
        >;

        Pipeline() = delete;
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        Pipeline(PipelineLayout& layout,
                 UniquePipelineHandleType pipeline,
                 vk::PipelineBindPoint bindPoint);
        Pipeline(Pipeline&&) noexcept = default;
        ~Pipeline() = default;

        Pipeline& operator=(Pipeline&&) noexcept = default;

        auto operator*() const noexcept -> vk::Pipeline;
        auto get() const noexcept -> vk::Pipeline;

        /**
         * @brief Bind the pipeline and its layout's resources
         *
         * Does not bind static descriptor sets defined by a DescriptorID.
         * You need a DescriptorRegistry for that.
         *
         * @param vk::CommandBuffer cmdBuf The command buffer to record the
         *                                 pipeline bind to.
         */
        void bind(vk::CommandBuffer cmdBuf) const;

        /**
         * @brief Bind the pipeline and all its layout's resources
         *
         * @param vk::CommandBuffer cmdBuf The command buffer to record the
         *                                 pipeline bind to.
         */
        void bind(vk::CommandBuffer cmdBuf, const ResourceStorage& descStorage) const;

        auto getLayout() noexcept -> PipelineLayout&;
        auto getLayout() const noexcept -> const PipelineLayout&;

        auto getBindPoint() const -> vk::PipelineBindPoint;

    private:
        PipelineLayout* layout;
        UniquePipelineHandleType pipelineStorage;
        vk::Pipeline pipeline;
        vk::PipelineBindPoint bindPoint;
    };

    /**
     * @brief Create a compute shader pipeline
     *
     * A most basic helper to create a compute pipeline quickly.
     *
     * @note No equivalent function exists for graphics pipelines because
     * their creation is much more complex.
     */
    auto makeComputePipeline(const Device& device,
                             PipelineLayout& layout,
                             const std::vector<ui32>& code,
                             vk::PipelineCreateFlags flags = {},
                             const std::string& entryPoint = "main") -> Pipeline;
} // namespace trc
