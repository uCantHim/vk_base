#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <trc_util/Assert.h>

#include "trc/Types.h"
#include "trc/VulkanInclude.h"

namespace trc::shader
{
    struct ShaderProgramData;

    struct ShaderProgramRuntime
    {
        ShaderProgramRuntime(const ShaderProgramRuntime&) = default;
        ShaderProgramRuntime(ShaderProgramRuntime&&) noexcept = default;
        ShaderProgramRuntime& operator=(const ShaderProgramRuntime&) = default;
        ShaderProgramRuntime& operator=(ShaderProgramRuntime&&) noexcept = default;
        ~ShaderProgramRuntime() noexcept = default;

        explicit ShaderProgramRuntime(const ShaderProgramData& program);

        /**
         * @brief Create a copy of the shader runtime
         *
         * Creates a 'fork' of the current state of the object. The fork will
         * refer to the same shader resources, but different runtime values
         * (such as push constant default values) can be configured for it.
         *
         * This is just a wrapper for the copy constructor.
         */
        auto clone() const -> u_ptr<ShaderProgramRuntime>;

        /**
         * @brief Check if the shader program uses a push constant
         */
        bool hasPushConstant(ui32 pushConstantId) const;

        /**
         * @brief Set data for a specific push constant.
         *
         * @throw std::invalid_argument if `pushConstantId` is not present in
         *        the shader program.
         */
        void pushConstants(vk::CommandBuffer cmdBuf,
                           vk::PipelineLayout layout,
                           ui32 pushConstantId,
                           const void* data, size_t size) const;

        /**
         * @brief Set data for a specific push constant.
         *
         * @throw std::invalid_argument if `pushConstantId` is not present in
         *        the shader program.
         */
        template<typename T>
        void pushConstants(vk::CommandBuffer cmdBuf,
                           vk::PipelineLayout layout,
                           ui32 pushConstantId,
                           T&& value) const
        {
            pushConstants(cmdBuf, layout, pushConstantId, &value, sizeof(T));
        }

        void setPushConstantDefaultValue(ui32 pushConstantId, std::span<const std::byte> data);

        void uploadPushConstantDefaultValues(vk::CommandBuffer cmdBuf, vk::PipelineLayout layout);

        auto getDescriptorSetIndex(const std::string& name) const -> std::optional<ui32>;

    private:
        struct PushConstant
        {
            ui32 offset;
            vk::ShaderStageFlags stages;
        };

        static constexpr ui32 kUserIdNotUsed{ std::numeric_limits<ui32>::max() };

        s_ptr<std::vector<PushConstant>> pc;
        s_ptr<std::unordered_map<std::string, ui32>> descriptorSetIndices;

        std::vector<std::pair<ui32, std::vector<std::byte>>> pushConstantData;
    };
} // namespace trc::shader
