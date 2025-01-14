#pragma once

#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

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

        auto addRaygenGroup(const fs::path& raygenPath) -> Self&;
        auto addMissGroup(const fs::path& missPath) -> Self&;
        auto addTrianglesHitGroup(const fs::path& closestHitPath,
                                  const fs::path& anyHitPath) -> Self&;
        auto addProceduralHitGroup(const fs::path& intersectionPath,
                                   const fs::path& closestHitPath,
                                   const fs::path& anyHitPath) -> Self&;
        auto addCallableGroup(const fs::path& callablePath) -> Self&;

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
        auto addShaderModule(const fs::path& path) -> vk::ShaderModule;
        auto addPipelineStage(vk::ShaderModule _module, vk::ShaderStageFlagBits stage) -> ui32;
        auto addShaderGroup(vk::RayTracingShaderGroupTypeKHR type)
            -> vk::RayTracingShaderGroupCreateInfoKHR&;

        const Device& device;
        const vk::DispatchLoaderDynamic& dl;

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
