#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "trc/ShaderPath.h"
#include "trc/Types.h"
#include "trc/core/Instance.h"
#include "trc/core/Pipeline.h"
#include "trc/ray_tracing/ShaderBindingTable.h"

namespace trc::rt
{
    constexpr vk::ShaderStageFlags ALL_RAY_PIPELINE_STAGE_FLAGS{
        vk::ShaderStageFlagBits::eRaygenKHR
        | vk::ShaderStageFlagBits::eCallableKHR
        | vk::ShaderStageFlagBits::eAnyHitKHR
        | vk::ShaderStageFlagBits::eClosestHitKHR
        | vk::ShaderStageFlagBits::eIntersectionKHR
    };

    /**
     * @brief Builder for ray tracing pipeline and shader binding table
     */
    class RayTracingPipelineBuilder
    {
    public:
        using Self = RayTracingPipelineBuilder;

        explicit RayTracingPipelineBuilder(const ::trc::Instance& instance);

        /**
         * Start an entry in the shader binding table. All shader groups
         * added after a call to this function will be grouped into the
         * same shader binding table entry.
         *
         * Finish an entry with a call to endTableEntry.
         *
         * If his function is not called, individual calls to add{*}Group
         * functions create a single entry for the added group.
         */
        auto beginTableEntry() -> Self&;

        /**
         * Finish the current shader binding table entry. Once this
         * function has been called, it is impossible to go back and edit
         * an entry.
         */
        auto endTableEntry() -> Self&;

        auto addRaygenGroup(const ShaderPath& raygenPath) -> Self&;
        auto addRaygenGroup(const std::vector<ui32>& raygenCode) -> Self&;

        auto addMissGroup(const ShaderPath& missPath) -> Self&;
        auto addMissGroup(const std::vector<ui32>& missCode) -> Self&;

        auto addTrianglesHitGroup(std::optional<ShaderPath> closestHitPath,
                                  std::optional<ShaderPath> anyHitPath) -> Self&;
        auto addTrianglesHitGroup(std::optional<std::vector<ui32>> closestHitCode,
                                  std::optional<std::vector<ui32>> anyHitCode) -> Self&;

        auto addProceduralHitGroup(std::optional<ShaderPath> intersectionPath,
                                   std::optional<ShaderPath> closestHitPath,
                                   std::optional<ShaderPath> anyHitPath) -> Self&;
        auto addProceduralHitGroup(std::optional<std::vector<ui32>> intersectionCode,
                                   std::optional<std::vector<ui32>> closestHitCode,
                                   std::optional<std::vector<ui32>> anyHitCode) -> Self&;

        auto addCallableGroup(const ShaderPath& callablePath) -> Self&;
        auto addCallableGroup(const std::vector<ui32>& callableCode) -> Self&;

        /**
         * @brief Use settings to build a pipeline and a shader binding table
         *
         * Is is advised to specify an allocator that allocates from a
         * memory pool for the `alloc` parameter. Otherwise, each entry in
         * the table gets its own memory allocation.
         *
         * The allocator must allocate memory with the
         * vk::MemoryAllocateFlagBits::eDeviceAddress flag set!
         */
        auto build(ui32 maxRecursionDepth,
                   PipelineLayout& layout,
                   const DeviceMemoryAllocator alloc
                       = DefaultDeviceMemoryAllocator{ vk::MemoryAllocateFlagBits::eDeviceAddress })
            -> std::pair<Pipeline, ShaderBindingTable>;

    private:
        static auto loadShader(const ShaderPath& path) -> std::vector<ui32>;

        auto addShaderModule(const std::vector<ui32>& code) -> vk::ShaderModule;
        auto addPipelineStage(vk::ShaderModule _module, vk::ShaderStageFlagBits stage) -> ui32;
        auto addShaderGroup(vk::RayTracingShaderGroupTypeKHR type)
            -> vk::RayTracingShaderGroupCreateInfoKHR&;

        const Device& device;
        const VulkanDispatchLoaderDynamic& dl;

        // Need to be kept alive for the stage create infos
        std::vector<vk::UniqueShaderModule> shaderModules;

        std::vector<vk::PipelineShaderStageCreateInfo> pipelineStages;
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;

        bool hasActiveEntry{ false };
        ui32 currentEntrySize{ 0 };
        std::vector<ui32> sbtEntries;
    };

	auto inline buildRayTracingPipeline(const ::trc::Instance& instance) -> RayTracingPipelineBuilder
	{
		return RayTracingPipelineBuilder{ instance };
	}
} // namespace trc::rt
