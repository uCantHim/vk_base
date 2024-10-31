#pragma once

#include <limits>
#include <vector>

#include <trc_util/Assert.h>

#include "trc/Types.h"
#include "trc/VulkanInclude.h"
#include "trc/core/Pipeline.h"

namespace trc
{
    struct MaterialRuntime
    {
        MaterialRuntime(Pipeline::ID pipeline,
                        const s_ptr<std::vector<ui32>>& pcOffsets,
                        const s_ptr<std::vector<vk::ShaderStageFlags>>& pcStages);

        auto getPipeline() const -> Pipeline::ID;

        bool hasPushConstant(ui32 pushConstantId) const
        {
            return pcOffsets->size() > pushConstantId
                && pcOffsets->at(pushConstantId) != std::numeric_limits<ui32>::max();
        }

        /**
         * @brief Set data for a specific push constant.
         *
         * @throw std::invalid_argument if `pushConstantId` is not present in
         *        the material's shader module.
         */
        void pushConstants(vk::CommandBuffer cmdBuf,
                           vk::PipelineLayout layout,
                           ui32 pushConstantId,
                           const std::byte* data, size_t size) const
        {
            assert_arg(hasPushConstant(pushConstantId));
            cmdBuf.pushConstants(layout, pcShaderStages->at(pushConstantId),
                                 pcOffsets->at(pushConstantId),
                                 size, data);
        }

        /**
         * @brief Set data for a specific push constant.
         *
         * @throw std::invalid_argument if `pushConstantId` is not present in
         *        the material's shader module.
         */
        template<typename T>
        void pushConstants(vk::CommandBuffer cmdBuf,
                           vk::PipelineLayout layout,
                           ui32 pushConstantId,
                           T&& value) const
        {
            assert_arg(hasPushConstant(pushConstantId));
            cmdBuf.pushConstants<T>(layout, pcShaderStages->at(pushConstantId),
                                    pcOffsets->at(pushConstantId), value);
        }

        void setPushConstantDefaultValue(ui32 pushConstantId,
                                         std::span<const std::byte> data)
        {
            pushConstantData.emplace_back(
                pushConstantId,
                std::vector<std::byte>{ data.begin(), data.end() }
            );
        }

        void uploadPushConstantData(vk::CommandBuffer cmdBuf, vk::PipelineLayout layout)
        {
            for (const auto& [id, data] : pushConstantData) {
                pushConstants(cmdBuf, layout, id, data.data(), data.size());
            }
        }

    private:
        Pipeline::ID pipeline;
        s_ptr<std::vector<ui32>> pcOffsets;
        s_ptr<std::vector<vk::ShaderStageFlags>> pcShaderStages;

        std::vector<std::pair<ui32, std::vector<std::byte>>> pushConstantData;
    };
} // namespace trc
