#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
namespace fs = std::filesystem;

#include "Logging.h"
#include "trc/VulkanInclude.h"

namespace trc
{

auto getRequiredValidationLayers() -> std::vector<const char*>;

/**
 * @brief A Vulkan debug configuration
 *
 * Is an empty object if TRC_DEBUG is not defined.
 *
 * Respects the macro definition `TRC_DEBUG_THROW_ON_VALIDATION_ERROR`, as well
 * as the following environment variables:
 *   - 'TORCH_DEBUG_LOG_DIR': Directory in which Vulkan debug logs are written.
 *    Default is './vulkan_logs'.
 *   - 'TORCH_DEBUG_THROW_ON_VALIDATION_ERROR': Vulkan validation errors throw a
 *    `std::runtime_error` if this variable is set to any value.
 */
class VulkanDebug
{
public:
    VulkanDebug(const VulkanDebug&) = delete;
    VulkanDebug(VulkanDebug&&) noexcept = delete;
    VulkanDebug& operator=(const VulkanDebug&) = delete;
    VulkanDebug& operator=(VulkanDebug&&) noexcept = delete;

    explicit VulkanDebug(vk::Instance instance);
    ~VulkanDebug() noexcept = default;

private:
    struct VulkanDebugLogger
    {
        VulkanDebugLogger();

        fs::path debugLogDir{ TRC_DEBUG_VULKAN_LOGS_DIR };
        bool throwOnValidationError;

        std::ofstream vkErrorLogFile;
        std::ofstream vkWarningLogFile;
        std::ofstream vkInfoLogFile;
        std::ofstream vkVerboseLogFile;
        Logger<log::LogLevel::eError> vkErrorLog{ vkErrorLogFile };
        Logger<log::LogLevel::eWarning> vkWarningLog{ vkWarningLogFile };
        Logger<log::LogLevel::eInfo> vkInfoLog{ vkInfoLogFile };
        Logger<log::LogLevel::eDebug> vkVerboseLog{ vkVerboseLogFile };

        void vulkanDebugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            vk::DebugUtilsMessageTypeFlagsEXT messageType,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData
        );
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallbackWrapper(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData
    );

#ifdef TRC_DEBUG
    const vk::Instance instance; // Required for messenger destruction
    VulkanDispatchLoaderDynamic dispatcher;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT, VulkanDispatchLoaderDynamic> debugMessenger;

    std::unique_ptr<VulkanDebugLogger> debugLogger;
#endif
};

} // namespace trc
